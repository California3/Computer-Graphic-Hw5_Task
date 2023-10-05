#include <fstream>
#include "Vector.hpp"
#include "Renderer.hpp"
#include "Scene.hpp"
#include <optional>

inline float deg2rad(const float &deg)
{ return deg * M_PI/180.0; }

// Compute reflection direction
Vector3f reflect(const Vector3f &I, const Vector3f &N)
{
    return I - 2 * dotProduct(I, N) * N;
}

// [comment]
// Compute refraction direction using Snell's law
//
// We need to handle with care the two possible situations:
//
//    - When the ray is inside the object
//
//    - When the ray is outside.
//
// If the ray is outside, you need to make cosi positive cosi = -N.I
//
// If the ray is inside, you need to invert the refractive indices and negate the normal N
// [/comment]
Vector3f refract(const Vector3f &I, const Vector3f &N, const float &ior)
{
    float cosi = clamp(-1, 1, dotProduct(I, N));
    float etai = 1, etat = ior;
    Vector3f n = N;
    if (cosi < 0) { cosi = -cosi; } else { std::swap(etai, etat); n= -N; }
    float eta = etai / etat;
    float k = 1 - eta * eta * (1 - cosi * cosi);
    return k < 0 ? 0 : eta * I + (eta * cosi - sqrtf(k)) * n;
}

// [comment]
// Compute Fresnel equation
//
// \param I is the incident view direction
//
// \param N is the normal at the intersection point
//
// \param ior is the material refractive index
// [/comment]
float fresnel(const Vector3f &I, const Vector3f &N, const float &ior)
{
    float cosi = clamp(-1, 1, dotProduct(I, N));
    float etai = 1, etat = ior;
    if (cosi > 0) {  std::swap(etai, etat); }
    // Compute sini using Snell's law
    float sint = etai / etat * sqrtf(std::max(0.f, 1 - cosi * cosi));
    // Total internal reflection
    if (sint >= 1) {
        return 1;
    }
    else {
        float cost = sqrtf(std::max(0.f, 1 - sint * sint));
        cosi = fabsf(cosi);
        float Rs = ((etat * cosi) - (etai * cost)) / ((etat * cosi) + (etai * cost));
        float Rp = ((etai * cosi) - (etat * cost)) / ((etai * cosi) + (etat * cost));
        return (Rs * Rs + Rp * Rp) / 2;
    }
    // As a consequence of the conservation of energy, transmittance is given by:
    // kt = 1 - kr;
}

// [comment]
// Returns true if the ray intersects an object, false otherwise.
//
// \param orig is the ray origin
// \param dir is the ray direction
// \param objects is the list of objects the scene contains
// \param[out] tNear contains the distance to the cloesest intersected object.
// \param[out] index stores the index of the intersect triangle if the interesected object is a mesh.
// \param[out] uv stores the u and v barycentric coordinates of the intersected point
// \param[out] *hitObject stores the pointer to the intersected object (used to retrieve material information, etc.)
// \param isShadowRay is it a shadow ray. We can return from the function sooner as soon as we have found a hit.
// [/comment]
std::optional<hit_payload> trace(
        const Vector3f &orig, const Vector3f &dir,
        const std::vector<std::unique_ptr<Object> > &objects)
{
    float tNear = kInfinity;
    std::optional<hit_payload> payload;
    for (const auto & object : objects)
    {
        float tNearK = kInfinity;
        uint32_t indexK;
        Vector2f uvK;
        if (object->intersect(orig, dir, tNearK, indexK, uvK) && tNearK < tNear)
        {
            payload.emplace();
            payload->hit_obj = object.get();
            payload->tNear = tNearK;
            payload->index = indexK;
            payload->uv = uvK;
            tNear = tNearK;
        }
    }

    return payload;
}

// [comment]
// Implementation of the Whitted-style light transport algorithm (E [S*] (D|G) L)
//
// This function is the function that compute the color at the intersection point
// of a ray defined by a position and a direction. Note that thus function is recursive (it calls itself).
//
// If the material of the intersected object is either reflective or reflective and refractive,
// then we compute the reflection/refraction direction and cast two new rays into the scene
// by calling the castRay() function recursively. When the surface is transparent, we mix
// the reflection and refraction color using the result of the fresnel equations (it computes
// the amount of reflection and refraction depending on the surface normal, incident view direction
// and surface refractive index).
//
// If the surface is diffuse/glossy we use the Phong illumation model to compute the color
// at the intersection point.
// [/comment]
Vector3f castRay(
        const Vector3f &orig, const Vector3f &dir, const Scene& scene,
        int depth)
{

    Vector3f hitColor = scene.backgroundColor;

    if (depth > scene.maxDepth) {
        return hitColor;
    }

    if (auto payload = trace(orig, dir, scene.get_objects()); payload)
    {
        Vector3f hitPoint = orig + dir * payload->tNear;
        Vector3f N; // normal
        Vector2f st; // st coordinates
        payload->hit_obj->getSurfaceProperties(hitPoint, dir, payload->index, payload->uv, N, st);
        switch (payload->hit_obj->materialType) {
            case GLASS:
            {
                Vector3f reflectionDirection = normalize(reflect(dir, N));
                Vector3f refractionDirection = normalize(refract(dir, N, payload->hit_obj->ior));
                Vector3f reflectionRayOrig = (dotProduct(reflectionDirection, N) < 0) ?
                                             hitPoint - N * scene.epsilon :
                                             hitPoint + N * scene.epsilon;
                Vector3f refractionRayOrig = (dotProduct(refractionDirection, N) < 0) ?
                                             hitPoint - N * scene.epsilon :
                                             hitPoint + N * scene.epsilon;
                Vector3f reflectionColor = castRay(reflectionRayOrig, reflectionDirection, scene, depth + 1);
                Vector3f refractionColor = castRay(refractionRayOrig, refractionDirection, scene, depth + 1);
                float kr = fresnel(dir, N, payload->hit_obj->ior);
                hitColor = reflectionColor * kr + refractionColor * (1 - kr);
                // TODO: Task1.5, open question
                // Modify GLASS case to add color tinting to GLASS
                // comment out modified codes and undo changes before bonus task

                // Guangming Zeng:
                // mix with yellow color
                // hitColor = hitColor * 0.8 + Vector3f(1, 1, 0) * 0.2;

                break;
            }

#ifdef BONUS_TASK_1
            case PLASTIC:
            {
                //[comment]
                // Bonus task 1: implement the ideal diffuse shading without specular component
                // unlike the Phong shading below, we shall treat lights as point lights
                // that is, the incident energy is given by IncidentEnergy = LightIntensity / (Distance^2),
                // where Distance is the Euclidean distance between light source and hit point

                // The final hitColor should be hitColor = diffuseColor * IncidentEnergy * cos(incident_angle)
                // Note the factor Kd is never used here
                Vector3f lightAmt = 0;
                Vector3f shadowPointOrig = (dotProduct(dir, N) < 0) ?
                                           hitPoint + N * scene.epsilon :
                                           hitPoint - N * scene.epsilon;

                for (auto& light : scene.get_lights()) {
                    Vector3f lightDir = light->position - hitPoint;
                    // square of the distance between hitPoint and the light
                    float lightDistance2 = dotProduct(lightDir, lightDir);
                    lightDir = normalize(lightDir);
                    float LdotN = std::max(0.f, dotProduct(lightDir, N)); // cos(incident_angle)

                    // is the point in shadow, and is the nearest occluding object closer to the object than the light itself?
                    auto shadow_res = trace(shadowPointOrig, lightDir, scene.get_objects());
                    bool inShadow = shadow_res && (shadow_res->tNear * shadow_res->tNear < lightDistance2);

                    Vector3f IncidentEnergy = light->intensity / (lightDistance2);

                    lightAmt += inShadow ? 0 : IncidentEnergy * LdotN; // IncidentEnergy * cos(incident_angle)
                }

                hitColor = payload->hit_obj->evalDiffuseColor(st) * lightAmt; // diffuseColor * IncidentEnergy * cos(incident_angle)
                break;
            }
#endif
#ifdef BONUS_TASK_2
            case MIRROR:
            {
                //[comment]
                // Bonus task 2: implement a mirror reflection shading 
                // unlike the Phong shading below, we shall treat lights as point lights
                // that is, the incident energy is given by IncidentEnergy = LightIntensity / (Distance^2),
                // where Distance is the Euclidean distance between light source and hit point

                // use the formula hitColor = mirrorColor + specularColor, hitColor is the sum of two subcomponents: 
                //    - mirrorColor: this is the mirror reflection color by tracing a reflection ray
                //                   similar to whats done in GLASS material, however without the refraction
                //                   component. That is, mirrorColor = castRay(reflection) * kr
                //    - specularColor: this is the same as the specular component in Phong model below,
                //                     however you should modify it so that IncidentEnergy is attenuated by 
                //                     distance. That is, 
                //                     specularColor = IncidentEnergy * Ks * (<reflected_ray, view_dir>^specularExponent)
                //                     where <v1,v2> is the nonnegative dot product between two vectors v1 and v2
                Vector3f mirrorColor = 0, specularColor = 0;

                // mirror reflection same in GLASS case
                Vector3f reflectionDirection = normalize(reflect(dir, N));
                Vector3f reflectionRayOrig = (dotProduct(reflectionDirection, N) < 0) ?
                                             hitPoint - N * scene.epsilon :
                                             hitPoint + N * scene.epsilon;
                Vector3f reflectionColor = castRay(reflectionRayOrig, reflectionDirection, scene, depth + 1);
                float kr = fresnel(dir, N, payload->hit_obj->ior);

                // specularColor component in Phong model
                for (auto& light : scene.get_lights()) {
                    Vector3f lightDir = light->position - hitPoint;

                    // square of the distance between hitPoint and the light
                    float lightDistance2 = dotProduct(lightDir, lightDir);
                    lightDir = normalize(lightDir);
                    Vector3f IncidentEnergy = light->intensity /  (lightDistance2 * 4 * M_PI);

                    Vector3f reflectionDirection = reflect(-lightDir, N);
                    specularColor += powf(std::max(0.f, -dotProduct(reflectionDirection, dir)),
                        payload->hit_obj->specularExponent) * IncidentEnergy;
                }

                hitColor = reflectionColor * kr + specularColor * payload->hit_obj->Ks;
                break;
            }
#endif

            default: // Phong shading model as default for PLASTIC and MIRROR
            {
                // [comment]
                // We use the Phong illumation model int the default case. The phong model
                // is composed of a diffuse and a specular reflection component.
                // [/comment]
                Vector3f lightAmt = 0, specularColor = 0;
                Vector3f shadowPointOrig = (dotProduct(dir, N) < 0) ?
                                           hitPoint + N * scene.epsilon :
                                           hitPoint - N * scene.epsilon;
                // [comment]
                // Loop over all lights in the scene and sum their contribution up
                // We also apply the lambert cosine law
                // [/comment]
                for (auto& light : scene.get_lights()) {
                    Vector3f lightDir = light->position - hitPoint;
                    // square of the distance between hitPoint and the light
                    float lightDistance2 = dotProduct(lightDir, lightDir);
                    lightDir = normalize(lightDir);
                    float LdotN = std::max(0.f, dotProduct(lightDir, N));
                    // is the point in shadow, and is the nearest occluding object closer to the object than the light itself?
                    auto shadow_res = trace(shadowPointOrig, lightDir, scene.get_objects());
                    bool inShadow = shadow_res && (shadow_res->tNear * shadow_res->tNear < lightDistance2);

                    lightAmt += inShadow ? 0 : light->intensity * LdotN;
                    Vector3f reflectionDirection = reflect(-lightDir, N);

                    specularColor += powf(std::max(0.f, -dotProduct(reflectionDirection, dir)),
                        payload->hit_obj->specularExponent) * light->intensity;
                }

                hitColor = lightAmt * payload->hit_obj->evalDiffuseColor(st) * payload->hit_obj->Kd + specularColor * payload->hit_obj->Ks;
                break;
            }
        }
    }

    return hitColor;
}

// [comment]
// The main render function. This where we iterate over all pixels in the image, generate
// primary rays and cast these rays into the scene. The content of the framebuffer is
// saved to a file.
// [/comment]
void Renderer::Render(const Scene& scene)
{
    std::vector<Vector3f> framebuffer(scene.width * scene.height);

    float scale = std::tan(deg2rad(scene.fov * 0.5f));
    float imageAspectRatio = scene.width / (float)scene.height;

    // Use this variable as the eye position to start your rays.
    Vector3f eye_pos(0);

    int m = 0;
    for (int j = 0; j < scene.height; ++j)
    {
        for (int i = 0; i < scene.width; ++i)
        {
            // generate primary ray direction
            float x ;
            float y ;
            // TODO: Task1.2
            // Find the x and y positions of the current pixel to get the direction
            // vector that passes through it.
            // Also, don't forget to multiply both of them with the variable *scale*, and
            // x (horizontal) variable with the *imageAspectRatio*            
            
            // Guangming Zeng:
            // map (0~width, 0~height) to (-1~1, -1~1) then use scale and aspect ratio to get (x, y)
            // irrelevant to z.
            // suppose screen (0,0) is at the top left corner of the screen (image).
            // need flip y axis to make it consistent with the image.
            x = (2 * (i + 0.5) / (float)scene.width - 1) * scale * imageAspectRatio;
            y = - (2 * (j + 0.5) / (float)scene.height - 1) * scale;

            Vector3f dir = Vector3f(x, y, -1); // Don't forget to normalize this direction!
            dir = normalize(dir);

            framebuffer[m++] = castRay(eye_pos, dir, scene, 0);
        }
        UpdateProgress(j / (float)scene.height);
    }

    // save framebuffer to file
    FILE* fp = fopen("binary.ppm", "wb");
    (void)fprintf(fp, "P6\n%d %d\n255\n", scene.width, scene.height);
    for (auto i = 0; i < scene.height * scene.width; ++i) {
        static unsigned char color[3];
        color[0] = (unsigned char)(255 * clamp(0, 1, framebuffer[i].x));
        color[1] = (unsigned char)(255 * clamp(0, 1, framebuffer[i].y));
        color[2] = (unsigned char)(255 * clamp(0, 1, framebuffer[i].z));
        fwrite(color, 1, 3, fp);
    }
    fclose(fp);   
}