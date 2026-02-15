#include "App/App.h"

int main(int argc, char* argv[]) {
    try {
        App app(1600, 900, "Gaussian Splatting");
        app.InitializePLY(argv[1]);
        app.Run();
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}