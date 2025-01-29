using Sharpmake;

[Export]
public class LibPNG : Project
{
    public LibPNG()
    {
        Name = "LibPNG";
        AddTargets(new Target(Platform.win64, DevEnv.vs2022, Optimization.Debug | Optimization.Release));
        SourceRootPath = @"[project.SharpmakeCsPath]\src";
    }

    [Configure()]
    public void Configure(Configuration conf, Target target)
    {
        conf.IncludePaths.Add(@"[project.SharpmakeCsPath]\libpng");
        conf.TargetFileName = @"[project.SharpmakeCsPath]\libpng\build\" + target.Optimization.ToString() + ((target.Optimization == Optimization.Debug) ? @"\libpng16d" : @"\libpng16");
        conf.Output = Configuration.OutputType.Lib;
    }
}