using System;
using System.ComponentModel.Design;
using System.Globalization;
using System.Threading;
using System.Threading.Tasks;
using System.Windows.Forms;
using Microsoft.VisualStudio.Shell;
using Microsoft.VisualStudio.Shell.Interop;
using EnvDTE;
using Task = System.Threading.Tasks.Task;

namespace OpenEnclaveSDK
{
    /// <summary>
    /// Command handler
    /// </summary>
    internal sealed class ImportEnclaveCommand
    {
        /// <summary>
        /// Command ID.
        /// </summary>
        public const int CommandId = 0x0100;

        /// <summary>
        /// Command menu group (command set GUID).
        /// </summary>
        public static readonly Guid CommandSet = new Guid("6818d8e0-d425-4d85-a296-32e1a4b63fcc");

        /// <summary>
        /// VS Package that provides this command, not null.
        /// </summary>
        private readonly AsyncPackage package;

        /// <summary>
        /// Initializes a new instance of the <see cref="ImportEnclaveCommand"/> class.
        /// Adds our command handlers for menu (commands must exist in the command table file)
        /// </summary>
        /// <param name="package">Owner package, not null.</param>
        /// <param name="commandService">Command service to add command to, not null.</param>
        private ImportEnclaveCommand(AsyncPackage package, OleMenuCommandService commandService)
        {
            this.package = package ?? throw new ArgumentNullException(nameof(package));
            commandService = commandService ?? throw new ArgumentNullException(nameof(commandService));

            var menuCommandID = new CommandID(CommandSet, CommandId);
            var menuItem = new MenuCommand(this.Execute, menuCommandID);
            commandService.AddCommand(menuItem);
        }

        /// <summary>
        /// Gets the instance of the command.
        /// </summary>
        public static ImportEnclaveCommand Instance
        {
            get;
            private set;
        }

        /// <summary>
        /// Gets the service provider from the owner package.
        /// </summary>
        private Microsoft.VisualStudio.Shell.IAsyncServiceProvider ServiceProvider
        {
            get
            {
                return this.package;
            }
        }

        /// <summary>
        /// Initializes the singleton instance of the command.
        /// </summary>
        /// <param name="package">Owner package, not null.</param>
        public static async Task InitializeAsync(AsyncPackage package)
        {
            // Switch to the main thread - the call to AddCommand in ImportEnclaveCommand's constructor requires
            // the UI thread.
            await ThreadHelper.JoinableTaskFactory.SwitchToMainThreadAsync(package.DisposalToken);

            OleMenuCommandService commandService = await package.GetServiceAsync((typeof(IMenuCommandService))) as OleMenuCommandService;
            Instance = new ImportEnclaveCommand(package, commandService);
        }

        static Project GetActiveProject(DTE dte)
        {
            Project activeProject = null;

            Array activeSolutionProjects = dte.ActiveSolutionProjects as Array;
            if (activeSolutionProjects != null && activeSolutionProjects.Length > 0)
            {
                activeProject = activeSolutionProjects.GetValue(0) as Project;
            }

            return activeProject;
        }

        static ProjectItem FindProjectItem(Project project, string name)
        {
            foreach (ProjectItem item in project.ProjectItems)
            {
                if (item.Name == name)
                {
                    return item;
                }
            }
            return null;
        }

        /// <summary>
        /// This function is the callback used to execute the command when the menu item is clicked.
        /// See the constructor to see how the menu item is associated with this function using
        /// OleMenuCommandService service and MenuCommand class.
        /// </summary>
        /// <param name="sender">Event sender.</param>
        /// <param name="e">Event args.</param>
        private void Execute(object sender, EventArgs e)
        {
            ThreadHelper.ThrowIfNotOnUIThread();

            DTE dte = Package.GetGlobalService(typeof(SDTE)) as DTE;
            Project project = GetActiveProject(dte);

            var fileContent = string.Empty;
            var filePath = string.Empty;

            using (OpenFileDialog openFileDialog = new OpenFileDialog())
            {
                openFileDialog.InitialDirectory = ".";
                openFileDialog.Filter = "EDL files (*.EDL)|*.edl";
                openFileDialog.RestoreDirectory = true;

                if (openFileDialog.ShowDialog() == DialogResult.OK)
                {
                    // Get the path of specified file.
                    filePath = openFileDialog.FileName;

                    // Extract base name.
                    string baseName = System.IO.Path.GetFileNameWithoutExtension(filePath);

                    // Add the EDL file to the project.
                    ProjectItem sourceFilesFolder = FindProjectItem(project, "Source Files");
                    if (sourceFilesFolder == null)
                    {
                        sourceFilesFolder = project.ProjectItems.AddFolder("Source Files",
                            EnvDTE.Constants.vsProjectItemKindVirtualFolder);
                    }
                    ProjectItem edlItem = sourceFilesFolder.ProjectItems.AddFromFile(filePath);

                    // Add list of generated files to the project.
                    ProjectItem generatedFilesFolder = FindProjectItem(project, "Generated Files");
                    if (generatedFilesFolder == null)
                    {
                        generatedFilesFolder = project.ProjectItems.AddFolder("Generated Files",
                            EnvDTE.Constants.vsProjectItemKindVirtualFolder);
                    }
                    ProjectItem ucItem = generatedFilesFolder.ProjectItems.AddFromFile(baseName + "_u.c");
                    ProjectItem uhItem = generatedFilesFolder.ProjectItems.AddFromFile(baseName + "_u.h");

                    // Add nuget package to project.
                    // See https://stackoverflow.com/questions/41803738/how-to-programmatically-install-a-nuget-package/41895490#41895490

#if false
                    // Debugger info.
                    Configuration configuration = project.ConfigurationManager.ActiveConfiguration;
                    configuration.Properties.Item("StartAction").Value = VSLangProj.prjStartAction.prjStartActionProgram;
                    configuration.Properties.Item("StartProgram").Value = "your exe file";
                    configuration.Properties.Item("StartArguments").Value = "command line arguments";
#endif
                }
            }
        }
    }
}
