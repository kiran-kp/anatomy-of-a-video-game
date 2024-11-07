using Sharpmake;

[Generate]
public class BirdGameProject : Project
{
    public BirdGameProject()
    {
        Name = "BirdGame";
        AddTargets(new Target(Platform.win64, DevEnv.vs2022, Optimization.Debug | Optimization.Release));
        SourceRootPath = @"[project.SharpmakeCsPath]\src";
    }

    [Configure()]
    public void Configure(Configuration conf, Target target)
    {
        conf.ProjectFileName = "BirdGame";
        conf.ProjectPath = @"[project.SharpmakeCsPath]\generated";

        conf.IncludePaths.Add(@"[project.SharpmakeCsPath]\include");

        conf.Options.Add(Options.Vc.General.CharacterSet.Unicode);
        conf.Options.Add(Options.Vc.General.WarningLevel.Level3);
        conf.Options.Add(Options.Vc.General.TreatWarningsAsErrors.Enable);
        conf.Options.Add(Options.Vc.General.WindowsTargetPlatformVersion.Latest);

        conf.Options.Add(Options.Vc.Compiler.CppLanguageStandard.CPP17);
        conf.Options.Add(Options.Vc.Compiler.Exceptions.Enable);

        conf.Options.Add(Options.Vc.Linker.SubSystem.Windows);
        conf.Options.Add(Options.Vc.Linker.LargeAddress.SupportLargerThan2Gb);

        conf.LibraryFiles.Add("d3d12");
        conf.LibraryFiles.Add("dxgi");
        conf.LibraryFiles.Add("d3dcompiler");
    }
}

[Generate]
public class BirdGameSolution : Solution
{
    public BirdGameSolution()
    {
        Name = "BirdGame";
        AddTargets(new Target(Platform.win64, DevEnv.vs2022, Optimization.Debug | Optimization.Release));
    }

    [Configure()]
    public void Configure(Configuration conf, Target target)
    {
        conf.SolutionFileName = "BirdGame";
        conf.SolutionPath = @"[solution.SharpmakeCsPath]\generated";
        conf.AddProject<BirdGameProject>(target);
    }
}

public static class Main
{
    [Sharpmake.Main]
    public static void SharpmakeMain(Arguments arguments)
    {
        arguments.Generate<BirdGameSolution>();
    }
}