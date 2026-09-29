using Astra.Compilation;
foreach(var path in args)
{
    var result=new ProjectCompiler().Build(Path.GetFullPath(path));
    foreach(var d in result.Diagnostics) Console.WriteLine($"{d.File}:{d.Line}:{d.Column} {d.Code} {d.Message}");
    Console.WriteLine($"{path}: success={result.Success}; behaviorTypes={result.Project?.Types.Length}");
    if(!result.Success) Environment.ExitCode=1;
}
