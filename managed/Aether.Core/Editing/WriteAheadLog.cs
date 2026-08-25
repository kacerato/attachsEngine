using System.Text;

namespace Aether.Editing;

/// <summary>
/// Log de recuperação (write-ahead log) append-only: cada <see cref="IEditCommand"/> executado é
/// primeiro serializado e gravado aqui — com fsync, antes de <see cref="IEditCommand.Do"/> rodar (ver
/// <see cref="UndoStack.ExecuteWithWal"/>) — para que, se o editor crashar, reabrir o projeto possa
/// recuperar os comandos ainda não salvos a partir deste arquivo. É a materialização direta da
/// regra 1 do item 8 do CONVENCOES.md: "nunca perder trabalho do usuário".
///
/// <para><b>Formato de cada registro</b> (little-endian, via <see cref="BinaryWriter"/>/<see
/// cref="BinaryReader"/> padrão do BCL):</para>
/// <code>
/// int32  tamanhoDoPayload   // bytes que seguem, sem contar este cabeçalho de 8 bytes
/// uint32 checksum           // FNV-1a de 32 bits sobre o payload
/// byte[] payload            // string (tag, prefixada por tamanho) + bytes específicos do comando
/// </code>
/// O tamanho e o checksum vêm ANTES do payload de propósito: permitem validar um registro inteiro
/// (ele cabe no que sobrou do arquivo, e o conteúdo bate com o checksum) antes de tentar
/// interpretar um único byte dele como comando.
///
/// <para><b>Recuperação parcial ante escrita incompleta</b> (a queda de energia no meio de um
/// append que o enunciado pede para pensar): <see cref="Recover"/> lê registro por registro e para
/// — sem lançar exceção — no primeiro que:</para>
/// <list type="bullet">
/// <item>não tem nem os 8 bytes de cabeçalho completos restantes no arquivo (queda bem no início
/// do registro);</item>
/// <item>declara um <c>tamanhoDoPayload</c> maior do que o que sobrou de arquivo (queda no meio do
/// payload);</item>
/// <item>tem o payload inteiro presente, mas o checksum não bate (bit-rot, ou uma escrita parcial
/// que por coincidência preencheu o espaço declarado com lixo);</item>
/// <item>lança ao desserializar (um bug no desserializador de um comando específico não pode
/// derrubar a recuperação inteira — CONVENCOES.md item 8, regra 3).</item>
/// </list>
/// Em qualquer um desses casos, todos os registros válidos ANTERIORES ao problemático já estão na
/// lista de retorno — perder o último comando não salvo (o que estava sendo gravado exatamente no
/// instante da queda) é aceitável; perder a sessão inteira de edição por causa dele não é.
/// </summary>
public sealed class WriteAheadLog : IDisposable
{
    private const int HeaderSize = sizeof(int) + sizeof(uint); // tamanho (int32) + checksum (uint32)

    private readonly WalCommandRegistry _registry;
    private readonly FileStream _stream;
    private readonly object _lock = new();
    private bool _disposed;

    public string Path { get; }

    public WriteAheadLog(string path, WalCommandRegistry registry)
    {
        ArgumentException.ThrowIfNullOrEmpty(path);
        ArgumentNullException.ThrowIfNull(registry);
        Path = path;
        _registry = registry;
        // FileShare.Read: permite que Recover() abra o mesmo arquivo para leitura enquanto este
        // log ainda está aberto para escrita (útil para inspeção/diagnóstico sem parar o editor).
        _stream = new FileStream(path, FileMode.OpenOrCreate, FileAccess.ReadWrite, FileShare.Read);
        _stream.Seek(0, SeekOrigin.End); // continua de onde o log parou, nunca sobrescreve o que já existe
    }

    /// <summary>
    /// Serializa <paramref name="command"/> e o anexa ao log, com <c>fsync</c> (<see
    /// cref="FileStream.Flush(bool)"/> com <c>flushToDisk: true</c>) antes de devolver o controle.
    /// Isso é deliberadamente síncrono e bloqueante: a garantia do WAL só existe se "gravado"
    /// significa "fisicamente em disco", não "na fila de escrita do SO" — um <c>Flush()</c> sem
    /// forçar o disco poderia sobreviver a um crash do PROCESSO mas não a uma queda de energia, e
    /// achamos que a garantia deve valer para os dois casos.
    /// </summary>
    public void Append(IEditCommand command)
    {
        ArgumentNullException.ThrowIfNull(command);
        ObjectDisposedException.ThrowIf(_disposed, this);

        string tag = _registry.TagFor(command);
        byte[] payload = SerializePayload(tag, command);
        uint checksum = Fnv1a(payload);

        lock (_lock)
        {
            using var writer = new BinaryWriter(_stream, Encoding.UTF8, leaveOpen: true);
            writer.Write(payload.Length);
            writer.Write(checksum);
            writer.Write(payload);
            writer.Flush();
            _stream.Flush(flushToDisk: true);
        }
    }

    private byte[] SerializePayload(string tag, IEditCommand command)
    {
        using var buffer = new MemoryStream();
        using (var writer = new BinaryWriter(buffer, Encoding.UTF8, leaveOpen: true))
        {
            writer.Write(tag);
            _registry.Serialize(tag, command, writer);
        }
        return buffer.ToArray();
    }

    /// <summary>
    /// Checkpoint: chamado depois de um save de projeto bem-sucedido, quando todos os comandos
    /// gravados até aqui já estão refletidos no arquivo de projeto de verdade e não precisam mais
    /// ser reaplicados numa recuperação. Zera o log (<c>SetLength(0)</c>) em vez de deletar e
    /// recriar o arquivo, para não haver uma janela sem nenhum arquivo de log no caminho.
    /// </summary>
    public void Checkpoint()
    {
        ObjectDisposedException.ThrowIf(_disposed, this);
        lock (_lock)
        {
            _stream.SetLength(0);
            _stream.Seek(0, SeekOrigin.Begin);
            _stream.Flush(flushToDisk: true);
        }
    }

    /// <summary>
    /// Lê o log em <paramref name="path"/> do início ao fim e devolve, na ordem em que foram
    /// gravados, os comandos que precisam ser reaplicados para recuperar uma sessão que crashou
    /// sem checkpoint. Chamado antes de abrir um <see cref="WriteAheadLog"/> "ao vivo" para o mesmo
    /// arquivo (que continuaria anexando a partir do fim). Devolve lista vazia se o arquivo não
    /// existe (nenhum crash pendente) — não é um erro.
    /// </summary>
    public static List<IEditCommand> Recover(string path, WalCommandRegistry registry)
    {
        ArgumentException.ThrowIfNullOrEmpty(path);
        ArgumentNullException.ThrowIfNull(registry);

        var result = new List<IEditCommand>();
        if (!File.Exists(path)) return result;

        using var stream = new FileStream(path, FileMode.Open, FileAccess.Read, FileShare.ReadWrite);
        using var reader = new BinaryReader(stream, Encoding.UTF8, leaveOpen: true);

        while (true)
        {
            long recordStart = stream.Position;
            if (stream.Length - recordStart < HeaderSize) break; // cabeçalho incompleto: fim do log ou queda no início de um registro

            int payloadLength;
            uint expectedChecksum;
            try
            {
                payloadLength = reader.ReadInt32();
                expectedChecksum = reader.ReadUInt32();
            }
            catch (EndOfStreamException)
            {
                break; // defesa extra: mesmo caso acima, não deveria acontecer dado o check de tamanho
            }

            if (payloadLength < 0 || recordStart + HeaderSize + payloadLength > stream.Length)
                break; // tamanho declarado não cabe no que sobrou do arquivo: queda no meio do payload

            byte[] payload = reader.ReadBytes(payloadLength);
            if (payload.Length != payloadLength) break; // leitura curta: mesmo caso acima

            if (Fnv1a(payload) != expectedChecksum)
                break; // registro presente por inteiro, mas corrompido — para aqui, mantém o que já é confiável

            using var payloadStream = new MemoryStream(payload, writable: false);
            using var payloadReader = new BinaryReader(payloadStream, Encoding.UTF8, leaveOpen: true);
            try
            {
                string tag = payloadReader.ReadString();
                if (!registry.TryDeserialize(tag, payloadReader, out var command)) break; // tag desconhecida (log de versão mais nova?)
                result.Add(command);
            }
            catch (Exception)
            {
                // Um desserializador de comando com bug não pode derrubar a recuperação inteira —
                // trata como registro ilegível e para, preservando tudo que já foi lido antes.
                break;
            }
        }

        return result;
    }

    private static uint Fnv1a(ReadOnlySpan<byte> data)
    {
        const uint offsetBasis = 2166136261;
        const uint prime = 16777619;
        uint hash = offsetBasis;
        foreach (byte b in data)
        {
            hash ^= b;
            hash *= prime;
        }
        return hash;
    }

    public void Dispose()
    {
        if (_disposed) return;
        _disposed = true;
        _stream.Dispose();
    }
}
