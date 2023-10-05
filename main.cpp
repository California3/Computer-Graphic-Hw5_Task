#include "Scene.hpp"
#include "Sphere.hpp"
#include "Triangle.hpp"
#include "Light.hpp"
#include "Renderer.hpp"

// In the main function of the program, we create the scene (create objects and lights)
// as well as set the options for the render (image width and height, maximum recursion
// depth, field-of-view, etc.). We then call the render function().
int main()
{
    Scene scene(1280, 960);

    auto sph1 = std::make_unique<Sphere>(Vector3f(-1, 0, -12), 2);
    sph1->materialType = PLASTIC;
    sph1->diffuseColor = Vector3f(0.6, 0.7, 0.8);

    scene.Add(std::move(sph1));

    auto sph2 = std::make_unique<Sphere>(Vector3f(0.5, -0.5, -8), 1.5);
    sph2->ior = 2.0;
    sph2->materialType = GLASS;

    scene.Add(std::move(sph2));

    Vector3f verts[4] = {{-5,-3,-6}, {5,-3,-6}, {5,-3,-16}, {-5,-3,-16}};
    uint32_t vertIndex[6] = {0, 1, 3, 1, 2, 3};
    Vector2f st[4] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};
    auto mesh = std::make_unique<MeshTriangle>(verts, vertIndex, 2, st);
    mesh->materialType = PLASTIC;

    scene.Add(std::move(mesh));

    //// TODO: Task 1.4
    //// Add a cube by uncommenting lines below
    //// fill in 8 vertices of the cube, you may move or rotate it so it is more visible
    //// for example, by centering it at (0,0,-5) and rotating 20 degrees
    //// about y and z axes
    bool enable_cube = false;
    if(enable_cube){
        // center at (0, 0, 0)
        Vector3f cube_verts[8] = {{-1, -1, -1}, {-1, -1, 1}, {-1, 1, -1}, {-1, 1, 1},
                                {1, -1, -1}, {1, -1, 1}, {1, 1, -1}, {1, 1, 1}}; 

        // center at (0, 0, -5)
        for (int i = 0; i < 8; i++) {
            cube_verts[i].z -= 5;
        }

        // rotate 20 degrees about z axes
        float theta = 20.0 * M_PI / 180.0;
        for (int i = 0; i < 8; i++) {
            float x = cube_verts[i].x;
            float y = cube_verts[i].y;
            cube_verts[i].x = x * cos(theta) - y * sin(theta);
            cube_verts[i].y = x * sin(theta) + y * cos(theta);
        }
        // rotate 20 degrees about y axes
        for (int i = 0; i < 8; i++) {
            float x = cube_verts[i].x;
            float z = cube_verts[i].z;
            cube_verts[i].x = x * cos(theta) + z * sin(theta);
            cube_verts[i].z = -x * sin(theta) + z * cos(theta);
        }

        //// fill in 6x2 triangles of the cube, the orientation of triangle is determined 
        //// by ordering of vertices using the right hand rule
        uint32_t cube_vertIndex[36] = {0, 1, 2, 1, 3, 2, 
                                    1, 0, 4, 1, 4, 5, 
                                    0, 2, 4, 2, 6, 4, 
                                    1, 5, 3, 3, 5, 7, 
                                    2, 3, 6, 3, 7, 6, 
                                    4, 6, 5, 5, 6, 7}; 

        //// these textures are set to all zeros since they are never used
        Vector2f cube_st[8] = {{0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}};
        auto cube_mesh = std::make_unique<MeshTriangle>(cube_verts, cube_vertIndex, 12, cube_st);
        cube_mesh->ior = 3.0;
        cube_mesh->materialType = GLASS;
        scene.Add(std::move(cube_mesh));
    }
    //// end adding cube
    

#ifndef BONUS_TASK
    // Add light sources as directional lights
    scene.Add(std::make_unique<Light>(Vector3f(-10, 40, 20), 0.5));
    scene.Add(std::make_unique<Light>(Vector3f(20, 35, -12), 0.5));
#else
    // Add light sources as point lights, since there is a quadratic 
    // energy attenuation, we increase intensitis accordingly
    scene.Add(std::make_unique<Light>(Vector3f(-10, 40, 20), 10000));
    scene.Add(std::make_unique<Light>(Vector3f(20, 35, -12), 10000));
#endif

#ifdef BONUS_TASK_2
    // add a third ball made of mirror material
    auto sph3 = std::make_unique<Sphere>(Vector3f(-1, 0, -5), 0.5);
    sph3->ior = 3.5;
    sph3->Ks = 5.0;
    sph3->materialType = MIRROR;

    scene.Add(std::move(sph3));
#endif
    Renderer r;
    r.Render(scene);

    return 0;
}