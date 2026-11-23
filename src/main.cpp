#include "ProjectMSDLApplication.h"

#include <SDL2/SDL.h>

int RunProjectMRenderer(int argc, char* argv[])
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
    return pApp->run();
}


// The standalone SDL application remains available when Qt is disabled.
// The integrated Qt application calls RunProjectMRenderer from its child mode,
// so only one user-facing executable needs to be installed.
#ifndef THISIS395_VJ_EMBEDDED_RENDERER
int main(int argc, char* argv[])
{
    return RunProjectMRenderer(argc, argv);
}
#endif
