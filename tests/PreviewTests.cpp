#include <iostream>
namespace NPCStudio::Preview {
void GraphicsSmoke();
}
int main() {
    try {
        NPCStudio::Preview::GraphicsSmoke();
        std::cout << "WARP: shader compilation, frame/portrait PNG, camera/light changes, pipeline restore passed\n";
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
