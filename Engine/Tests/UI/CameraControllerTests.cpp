#include <doctest.h>

#include "Aquila/Foundation/Math/Math.h"
#include "Aquila/Rendering/CameraController.h"
#include "Aquila/Rendering/SceneFrameData.h"
#include "Aquila/Scene/Components/CameraComponent.h"
#include "Aquila/Scene/Components/TransformComponent.h"

#include "CanvasTestSupport.h"

using namespace Aquila;
using namespace Aquila::Rendering;
using Aquila::SceneManagement::Components::CameraComponent;
using Aquila::SceneManagement::Components::TransformComponent;

TEST_SUITE("CameraController") {
	TEST_CASE("a scene camera with the editor camera's pose gives the same view") {
		CameraController controller;
		const RenderView editor = controller.get_render_view();
		CameraComponent lens;
		lens.fov = controller.get_fov();
		lens.near_plane = 0.1F;
		lens.far_plane = 1000.F;
		TransformComponent transform(Vec3(0.F, 1.5F, -5.F));
		const RenderView from_transform = render_view_from_entity(lens, transform, controller.get_aspect());

		for (int column = 0; column < 4; ++column) {
			for (int row = 0; row < 4; ++row) {
				CHECK(from_transform.view[column][row] == doctest::Approx(editor.view[column][row]).epsilon(1e-4));
				CHECK(from_transform.projection[column][row] ==
					  doctest::Approx(editor.projection[column][row]).epsilon(1e-4));
			}
		}
		CHECK(Math::dot(from_transform.up, editor.up) == doctest::Approx(1.F));
		CHECK(Math::dot(from_transform.right, editor.right) == doctest::Approx(1.F));
	}

	TEST_CASE("scale on the transform does not leak into the view") {
		CameraComponent lens;
		TransformComponent transform(Vec3(2.F, 0.F, 0.F), Quaternion(1.F, 0.F, 0.F, 0.F), Vec3(3.F));
		const RenderView view = render_view_from_entity(lens, transform);
		CHECK(Math::length(Vec3(view.view[0])) == doctest::Approx(1.F));
		CHECK(view.position.x == doctest::Approx(2.F));
		CHECK(view.aspect == doctest::Approx(lens.aspect_ratio));
	}

	TEST_CASE("a parented camera looks along its world rotation") {
		CameraComponent lens;
		TransformComponent transform;
		transform.set_parent_matrix(glm::rotate(Mat4(1.F), Math::radians(90.F), Vec3(0.F, 1.F, 0.F)));
		const RenderView view = render_view_from_entity(lens, transform);
		CHECK(view.forward.x == doctest::Approx(1.F));
		CHECK(view.forward.z == doctest::Approx(0.F).epsilon(1e-4));
		CHECK(view.up.y == doctest::Approx(1.F));
	}

	TEST_CASE("fov is clamped and applied to the projection") {
		CanvasSingletonsScope singletons;
		CameraController controller;
		const F32 before = controller.get_render_view().projection[1][1];
		controller.set_fov(30.F);
		CHECK(controller.get_fov() == doctest::Approx(30.F));
		CHECK(std::abs(controller.get_render_view().projection[1][1]) > std::abs(before));
		controller.set_fov(500.F);
		CHECK(controller.get_fov() == doctest::Approx(179.F));
	}
}
