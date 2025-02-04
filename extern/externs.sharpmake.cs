using Sharpmake;

[Export]
public class LibPNG : Project
{
    public LibPNG()
    {
        Name = "LibPNG";
        AddTargets(new Target(Platform.win64, DevEnv.vs2022, Optimization.Debug | Optimization.Release));
    }

    [Configure()]
    public void Configure(Configuration conf, Target target)
    {
        conf.IncludePaths.Add(@"[project.SharpmakeCsPath]\libpng");
        conf.TargetFileName = @"[project.SharpmakeCsPath]\libpng\build\" + target.Optimization.ToString() + ((target.Optimization == Optimization.Debug) ? @"\libpng16_staticd" : @"\libpng16_static");
        conf.Output = Configuration.OutputType.Lib;
    }
}

[Export]
public class Zlib : Project
{
    public Zlib()
    {
        Name = "Zlib";
        AddTargets(new Target(Platform.win64, DevEnv.vs2022, Optimization.Debug | Optimization.Release));
    }

    [Configure()]
    public void Configure(Configuration conf, Target target)
    {
        conf.IncludePaths.Add(@"[project.SharpmakeCsPath]\zlib");
        conf.TargetFileName = @"[project.SharpmakeCsPath]\zlib\build\" + target.Optimization.ToString() + ((target.Optimization == Optimization.Debug) ? @"\zlibstaticd" : @"\zlibstatic");
        conf.Output = Configuration.OutputType.Lib;
    }
}