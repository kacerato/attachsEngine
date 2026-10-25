using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using System.Threading.Tasks;

namespace Aether.Interop;

/// <summary>
/// Exceção gerenciada que escapa de uma thread (tarefa, timer, thread própria)
/// encerra o processo: o runtime chama exit() e a mensagem vai para o stderr,
/// que no Android não aparece em lugar nenhum. O sintoma no aparelho era só um
/// "FORTIFY: pthread_mutex_lock called on a destroyed mutex" das threads de
/// renderização do sistema (os destrutores estáticos rodaram com elas vivas),
/// sem tombstone e sem pista do script.
/// <para>
/// Este inicializador registra, assim que o Aether.Core carrega, o tipo da
/// exceção, a mensagem e a pilha no logcat (tag Astra, prioridade FATAL) antes
/// de o runtime encerrar. Tarefas com exceção não observada não derrubam o
/// processo no .NET 8, mas também ficam no log (prioridade ERROR). Fora do
/// Android a mesma linha vai para o stderr.
/// </para>
/// </summary>
internal static class UnhandledExceptionLog
{
    private const int AndroidLogError = 6, AndroidLogFatal = 7;

    [DllImport("log", EntryPoint = "__android_log_write")]
    private static extern int AndroidLogWrite(int priority, string tag, string text);

#pragma warning disable CA2255 // inicializador de módulo é o ponto certo: roda antes de qualquer script
    [ModuleInitializer]
    internal static void Install()
#pragma warning restore CA2255
    {
        AppDomain.CurrentDomain.UnhandledException += (_, args) =>
            Write(AndroidLogFatal, "exceção não tratada encerra o processo", args.ExceptionObject as Exception);
        TaskScheduler.UnobservedTaskException += (_, args) =>
            Write(AndroidLogError, "exceção não observada numa tarefa", args.Exception);
    }

    internal static void Write(int priority, string context, Exception? error)
    {
        string text = $"[Managed] {context}: {error?.ToString() ?? "(sem objeto de exceção)"}";
        try
        {
            if (OperatingSystem.IsAndroid())
            {
                // O logcat corta cada registro em ~4 KB: a pilha vai em pedaços.
                for (int start = 0; start < text.Length; start += 3500)
                    AndroidLogWrite(priority, "Astra", text.Substring(start, Math.Min(3500, text.Length - start)));
                return;
            }
        }
        catch (Exception) { }
        try { Console.Error.WriteLine(text); } catch (Exception) { }
    }
}
