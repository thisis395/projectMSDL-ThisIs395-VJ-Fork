#include "ProjectMSDLApplication.h"

#include <SDL2/SDL.h>
#include <string>

int RunProjectMRenderer(int argc, char* argv[], bool verifyControlDir)
{
    Poco::AutoPtr<ProjectMSDLApplication> pApp = new ProjectMSDLApplication;
    try
    {
        pApp->init(argc, argv);
    }
    catch (Poco::Exception& exc)
    {
        pApp->logger().log(exc);
        return Poco::Util::Application::EXIT_CONFIG;
    }
    // Exercise the real POCO argument parser without needing an OpenGL context.
    if (verifyControlDir)
    {
        const auto parsed = pApp->CommandLineConfiguration()->getString("native.controlDir", "");
        return argc == 3 && std::string(argv[1]) == "--controlDir" &&
            !parsed.empty() && parsed == argv[2] ? EXIT_SUCCESS : EXIT_FAILURE;
    }
    return pApp->run();
}


// The standalone SDL application remains available when Qt is disabled.
// The integrated Qt application calls RunProjectMRenderer from its child mode,
// so only one user-facing executable needs to be installed.
#ifndef THISIS395_VJ_EMBEDDED_RENDERER
int main(int argc, char* argv[])
{
    return RunProjectMRenderer(argc, argv, false);
}
#endif
