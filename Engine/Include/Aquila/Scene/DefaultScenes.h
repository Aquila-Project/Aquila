#ifndef AQUILA_SCENE_DEFAULT_SCENES_H
#define AQUILA_SCENE_DEFAULT_SCENES_H

#include "Aquila/Foundation/PrimitiveTypes.h"
#include "Aquila/Scene/Entity.h"
#include "Aquila/Scene/Scene.h"

namespace Aquila::GFX {
class GfxContext;
}

namespace Aquila::Graphics {
class Material;
}

namespace Aquila::SceneManagement::Components {
struct MaterialComponent;
}

namespace Aquila::SceneManagement {

Ref<Graphics::Material> make_default_material(GFX::GfxContext &ctx);

/// Points `component.material` at what it should draw with: the asset's shared material (looked up again every
/// call, so an asset rebuilt with a new shader is picked up) or, without an asset, a material of its own built from
/// `embedded` once. Never leaves it null unless even the standard shader fails. Cheap when nothing changed.
void ensure_material(Components::MaterialComponent &component, GFX::GfxContext &ctx);

/// ensure_material() for every MaterialComponent. Application calls it from Scene::on_loaded so the first frame
/// doesn't compile everything.
void create_missing_materials(Scene &scene, GFX::GfxContext &ctx);

Entity spawn_default_camera(Scene &scene, F32 aspect_ratio);

void populate_demo_scene(Scene &scene, GFX::GfxContext &ctx, F32 aspect_ratio);

}

#endif
