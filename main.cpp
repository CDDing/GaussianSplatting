#include "App/App.h"

int main() {
    try {
        App app(1600, 900, "Gaussian Splatting");
        app.Run();
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
