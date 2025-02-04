using Sharpmake;

[Export]
public class WICTextureLoader : Project
{
    public WICTextureLoader()
    {
        Name = "WICTextureLoader";
        AddTargets(new Target(Platform.win64, DevEnv.vs2022, Optimization.Debug | Optimization.Release));
    }

    [Configure()]
    public void Configure(Configuration conf, Target target)
    {
        conf.IncludePaths.Add(@"[project.SharpmakeCsPath]\DirectXTex\WICTextureLoader");
        conf.IncludePaths.Add(@"[project.SharpmakeCsPath]\DirectXTex\Common");
        conf.Output = Configuration.OutputType.None;
    }
}