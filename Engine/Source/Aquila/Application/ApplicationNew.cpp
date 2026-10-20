#include "Aquila/Application/ApplicationNew.h"

#include <ranges>
#include "Aquila/Foundation/EnginePaths.h"
#include "Aquila/Foundation/Profiler.h"
#include "Aquila/Graphics/Material/MaterialAssets.h"
#include "Aquila/Foundation/JobSystem.h"
#include "Aquila/Graphics/Texture/TextureLibrary.h"
#include "Aquila/Platform/Filesystem/VirtualFileSystem.h"
#include "Aquila/Platform/Input.h"

#include "Aquila/GFX/GfxCommandList.h"
#include "Aquila/Graphics/Core/Renderer2D.h"
#include "Aquila/RHI/Vulkan/VulkanShaderCompiler.h"

#include "Aquila/Rendering/Systems/BloomSystem.h"
#include "Aquila/Rendering/Systems/GridSystem.h"
#include "Aquila/Rendering/Systems/OutlineSystem.h"
#include "Aquila/Rendering/Systems/ObjectPickingSystem.h"
#include "Aquila/Rendering/Systems/SkySystem.h"
#include "Aquila/Rendering/Systems/ToneMapSystem.h"
#include "Aquila/Rendering/Systems/FullscreenEffectSystem.h"
#include "Aquila/Rendering/Systems/TransparentSystem.h"
#include "Aquila/Rendering/Systems/LightCullingSystem.h"
#include "Aquila/Graphics/Material/MaterialFactory.h"
#include "Aquila/Graphics/Shader/ShaderHotReload.h"
#include "Aquila/Foundation/SharedConstants.h"
#include "Aquila/Scene/DefaultScenes.h"
#include "Aquila/Scene/EntityManager.h"

#include "Aquila/Rendering/Systems/DepthPrepassSystem.h"
#include "Aquila/Rendering/Systems/ShadowSystem.h"
#include "Aquila/Rendering/Systems/GeometrySystem.h"
#include "Aquila/Rendering/Systems/DebugViewSystem.h"
#include "Aquila/Rendering/Systems/ComputeTestSystem.h"
#include "Aquila/Foundation/FrameScheduler.h"
#include "Aquila/Platform/Filesystem/NativeFileSystem.h"
#include "Aquila/Platform/Filesystem/Filesystem.h"

namespace Aquila::Application {

using namespace SceneManagement;

namespace {
constexpr F32 k_resume_delta = 1.F / 60.F;
constexpr std::chrono::milliseconds k_hot_reload_interval{ 500 };

bool is_cursor_event(const Platform::Events::Event &event) {
	return (event.get_category() & Platform::Events::EventCategory::Mouse) &&
		!(event.get_category() & Platform::Events::EventCategory::MouseButton);
}

struct FrameGuard {
	AQUILA_NONCOPYABLE(FrameGuard);
	AQUILA_NONMOVEABLE(FrameGuard);
	explicit FrameGuard(bool &flag) : m_flag(flag) { m_flag = true; }
	~FrameGuard() { m_flag = false; }
	bool &m_flag;
};
} // namespace

Application::Application(const ApplicationSpec &spec) : m_spec(spec) {
	if (!spec.shaders_dir.empty()) {
		Foundation::EnginePaths::set_shaders_dir(spec.shaders_dir);
	}
	if (!spec.resources_dir.empty()) {
		Foundation::EnginePaths::set_resources_dir(spec.resources_dir);
	}

	m_timer = std::make_unique<Foundation::Stopwatch>();
	m_window = std::make_unique<Window>(spec.width, spec.height, spec.name, true, spec.start_hidden);
	Foundation::Profiler::Profiler::init();
	Foundation::JobSystem::init();
	Platform::Filesystem::VirtualFileSystem::init();
	Graphics::MaterialAssets::init();
	Graphics::TextureLibrary::init();

	m_window->set_event_callback([this](Platform::Events::Event &event) { route_window_event(event); });

	m_window->set_refresh_callback([this]() { internal_update(next_frame_delta()); });

	m_timer->start();
}

void Application::route_window_event(Platform::Events::Event &event) {
	Platform::Input::on_event(event);

	const auto *source = event.get_source();

	if (source == nullptr) {
		return;
	}

	if (source == m_window.get()) {
		internal_on_main_window_event(event);
		return;
	}

	for (auto &rw : m_secondary_windows) {
		if (source == rw->window.get()) {
			internal_on_secondary_window_event(*rw, event);
			return;
		}
	}
}

Application::~Application() {
	m_ctx->wait_idle();
	m_modules.clear();

	Foundation::JobSystem::shutdown();
	Graphics::TextureLibrary::shutdown();
	Graphics::MaterialAssets::shutdown();
	Platform::Filesystem::VirtualFileSystem::shutdown();
	Foundation::Profiler::Profiler::shutdown();
	Graphics::MaterialFactory::shutdown();
	Foundation::FrameScheduler::shutdown();

	m_scene.reset();
	m_render_pipeline.reset();
	m_secondary_windows.clear();
	m_secondary_batcher.reset();
	m_swapchain.reset();
	m_ctx.reset();

	Graphics::Shader::ShaderHotReload::shutdown();

	// TODO: move to a generic shader compiler abstraction
	RHI::VulkanShaderCompiler::shutdown();
}

void Application::run() {
	init_rendering(m_window->get_width(), m_window->get_height());
	m_scene = std::make_unique<Scene>("Main");
	m_scene->on_loaded.connect([this] { SceneManagement::create_missing_materials(*m_scene, *m_ctx); });
	on_init();
	attach_modules();

	m_frame_interval = 1.0 / Window::highest_refresh_rate();
	Foundation::FrameScheduler *scheduler = Foundation::FrameScheduler::get();
	while (m_running) {
		if (!scheduler->is_pending()) {
			m_render_pipeline->settle();
		}
		if (!scheduler->is_pending()) {
			m_resumed_from_idle = true;
			m_window->wait_events(idle_timeout());
		} else if (const F64 until_slot = seconds_until_frame_slot(); until_slot > 0.0) {
			m_window->wait_events(until_slot);
		} else {
			m_window->poll_events();
		}

		m_window->flush_pending_events();

		for (auto &rw : m_secondary_windows) {
			rw->window->flush_pending_events();
		}

		bool any_closing = false;
		for (const auto &rw : m_secondary_windows) {
			if (rw->window->should_close()) {
				any_closing = true;
				break;
			}
		}
		if (any_closing) {
			m_ctx->wait_idle();
			std::erase_if(m_secondary_windows, [scheduler](const Unique<RenderWindow> &rw) {
				if (!rw->window->should_close()) {
					return false;
				}
				if (rw->on_close) {
					rw->on_close();
				}
				Platform::Input::on_window_destroyed(rw->window.get());
				scheduler->forget(rw->window.get());
				return true;
			});
		}

		if (m_window->should_close()) {
			m_running = false;
			break;
		}

		poll_shader_hot_reload();

		if (seconds_until_frame_slot() <= 0.0 && scheduler->is_pending()) {
			m_last_frame_start = Foundation::now();
			const bool full_frame = scheduler->consume();
			const F32 delta_time = next_frame_delta();

			PROFILE_FRAME_BEGIN();
			if (full_frame) {
				internal_update(delta_time);
			} else {
				render_secondary_windows(false);
			}
			PROFILE_FRAME_END();
		}
	}

	m_ctx->wait_idle();
	if (m_play_state != PlayState::Stopped) {
		set_play_state(PlayState::Stopped);
	}
	detach_modules();
	on_shutdown();
}

F32 Application::next_frame_delta() {
	m_timer->tick();
	m_frame_delta = m_timer->get_delta_time();
	if (m_resumed_from_idle) {
		m_frame_delta = std::min(m_frame_delta, k_resume_delta);
		m_resumed_from_idle = false;
	}
	return m_frame_delta;
}

F64 Application::seconds_until_frame_slot() const {
	return m_frame_interval - Foundation::elapsed_seconds(m_last_frame_start, Foundation::now());
}

Option<F64> Application::idle_timeout() const {
	Option<F64> timeout = Foundation::FrameScheduler::get()->seconds_until_deadline();
	if (Graphics::Shader::ShaderHotReload::get()->is_enabled()) {
		const F64 until_check = std::max(Foundation::elapsed_seconds(Foundation::now(), m_next_hot_reload_check), 0.0);
		timeout = timeout ? std::min(*timeout, until_check) : until_check;
	}
	return timeout;
}

void Application::poll_shader_hot_reload() {
	const Foundation::TimePoint current = Foundation::now();
	if (current < m_next_hot_reload_check) {
		return;
	}
	m_next_hot_reload_check = current + k_hot_reload_interval;
	auto *hot_reload = Graphics::Shader::ShaderHotReload::get();
	hot_reload->tick();
	if (hot_reload->is_enabled() && Graphics::TextureLibrary::get()->poll_changes(*m_ctx)) {
		Foundation::FrameScheduler::get()->request_frame();
	}
}

void Application::attach_modules() {
	m_modules_attached = true;
	while (m_attached_modules < m_modules.size()) {
		attach_module(*m_modules[m_attached_modules]);
		++m_attached_modules;
	}
}

void Application::attach_module(IModule &module) {
	module.on_attach(m_engine_context);
	if (m_play_state != PlayState::Stopped) {
		module.on_play();
	}
	if (m_play_state == PlayState::Paused) {
		module.on_pause();
	}
}

void Application::detach_modules() {
	for (auto &m_module : std::views::reverse(m_modules)) {
		m_module->on_detach();
	}
	m_modules.clear();
	m_modules_attached = false;
	m_attached_modules = 0;
}

void Application::set_play_state(PlayState state) {
	const PlayState previous = std::exchange(m_play_state, state);
	if (previous == PlayState::Stopped) {
		m_game_input.clear();
	}
	const Usize count = m_attached_modules;
	for (Usize i = 0; i < count; ++i) {
		IModule &module = *m_modules[state == PlayState::Stopped ? count - 1 - i : i];
		switch (state) {
		case PlayState::Playing:
			if (previous == PlayState::Paused) {
				module.on_resume();
			} else {
				module.on_play();
			}
			break;
		case PlayState::Paused:
			module.on_pause();
			break;
		case PlayState::Stopped:
			module.on_stop();
			break;
		}
	}
	Foundation::FrameScheduler::get()->request_frame();
}

void Application::close() {
	m_running = false;
}

void Application::init_rendering(Uint32 width, Uint32 height) {
	// TODO: replace with a generic shader compiler abstraction
	RHI::VulkanShaderCompiler::initialize();
	Graphics::MaterialFactory::init();
	Graphics::Shader::ShaderHotReload::init();
	Graphics::Shader::ShaderHotReload::get()->enable(true);
	Foundation::FrameScheduler::init();
	Foundation::FrameScheduler::get()->set_wake([] { Window::wake(); });

	using namespace Platform::Filesystem;
	VirtualFileSystem::get()->mount("/resources",
									std::make_shared<NativeFileSystem>(Foundation::EnginePaths::resources_dir()));
	VirtualFileSystem::get()->mount("/shaders",
									std::make_shared<NativeFileSystem>(Foundation::EnginePaths::shaders_dir()));
	VirtualFileSystem::get()->mount("/app",
									std::make_shared<NativeFileSystem>(Platform::Filesystem::path_executable_dir()));

	m_ctx = GFX::GfxContext::create(*get_window().get_native_window());

	m_swapchain = m_ctx->create_swapchain({
		.width = width,
		.height = height,
		.format = RHI::TextureFormat::BGRA8,
		.image_count = 2,
		.vsync = false,
	});

	m_render_pipeline = std::make_unique<Rendering::RenderPipeline>(*m_ctx, width, height);
	m_render_width = width;
	m_render_height = height;
	m_renderer = &m_render_pipeline->add<Rendering::Renderer>();
	m_overlay_renderer = &m_render_pipeline->add<Rendering::OverlayRenderer>();

	m_renderer->add_system<Rendering::DepthPrepassSystem>();
	m_renderer->add_system<Rendering::ClusterComputeSystem>();
	m_renderer->add_system<Rendering::LightCullingSystem>();
	m_renderer->add_system<Rendering::ShadowSystem>();
	m_object_picking = &m_renderer->add_system<Rendering::ObjectPickingSystem>();
	m_renderer->add_system<Rendering::GeometrySystem>();
	m_renderer->add_system<Rendering::DebugViewSystem>();
	m_renderer->add_system<Rendering::SkySystem>();
	m_renderer->add_system<Rendering::TransparentSystem>();

	// post process
	m_renderer->add_system<Rendering::FullscreenEffectSystem>();
	m_renderer->add_system<Rendering::BloomSystem>();
	m_renderer->add_system<Rendering::ToneMapSystem>();
	m_renderer->add_system<Rendering::GridSystem>();
	m_renderer->add_system<Rendering::OutlineSystem>();

	m_secondary_batcher = std::make_unique<Graphics::Renderer2D>(*m_ctx);
}

RenderWindow &Application::create_secondary_window(Uint32 width, Uint32 height, const std::string &title) {
	auto rw = std::make_unique<RenderWindow>();
	rw->window = std::make_unique<Window>(width, height, title, false);
	rw->window->set_event_callback([this](Platform::Events::Event &event) { route_window_event(event); });
	rw->window->set_refresh_callback([this, p = rw.get()]() { render_one_secondary_window(*p); });
	rw->swapchain = m_ctx->create_swapchain({
		.width = width,
		.height = height,
		.format = RHI::TextureFormat::BGRA8,
		.image_count = 2,
		.vsync = false,
		.native_window_handle = rw->window->get_native_window(),
	});

	RenderWindow &ref = *rw;
	m_secondary_windows.push_back(std::move(rw));
	return ref;
}

Rendering::RenderWindowId Application::create_window(Uint32 width, Uint32 height, const std::string &title,
													 Rendering::RenderWindowCallbacks callbacks) {
	RenderWindow &rw = create_secondary_window(width, height, title);
	rw.on_update = std::move(callbacks.on_update);
	rw.on_render = std::move(callbacks.on_render);
	rw.on_event = std::move(callbacks.on_event);
	rw.on_close = std::move(callbacks.on_close);
	rw.cursor = std::move(callbacks.cursor);
	rw.forward_events = callbacks.forward_events;
	return rw.window.get();
}

void Application::request_close(Rendering::RenderWindowId window) {
	static_cast<const Window *>(window)->request_close();
}

Vec2 Application::get_window_position(Rendering::RenderWindowId window) const {
	return static_cast<const Window *>(window)->get_position();
}

Vec2 Application::get_window_size(Rendering::RenderWindowId window) const {
	const auto *native = static_cast<const Window *>(window);
	return { static_cast<F32>(native->get_width()), static_cast<F32>(native->get_height()) };
}

void Application::set_window_position(Rendering::RenderWindowId window, Vec2 position) {
	static_cast<const Window *>(window)->set_position(position);
}

void Application::set_window_min_size(Rendering::RenderWindowId window, Vec2 size) {
	static_cast<const Window *>(window)->set_min_size(size);
}

void Application::ensure_window_targets(RenderWindow &rw, Uint32 width, Uint32 height) {
	if (!rw.msaa_color) {
		rw.msaa_color = m_ctx->create_texture({
			.width = width,
			.height = height,
			.format = RHI::TextureFormat::BGRA8,
			.usage = RHI::TextureUsage::ColorAttachment,
			.samples = RHI::SampleCount::X4,
			.debug_name = "SecondaryUIMSAA",
		});
		rw.render_pass = m_ctx->create_render_pass({
			.color_attachments = { {
				.texture = &rw.msaa_color->get_rhi(),
				.load_op = RHI::AttachmentLoadOp::Clear,
				.store_op = RHI::AttachmentStoreOp::DontCare,
			} },
			.use_swapchain_as_resolve = true,
			.debug_name = "SecondaryUI",
		});
	}
}

void Application::render_secondary_windows(bool all) {
	Foundation::FrameScheduler *scheduler = Foundation::FrameScheduler::get();
	std::vector<RenderWindow *> due;
	for (auto &rw : m_secondary_windows) {
		if (scheduler->consume(rw->window.get()) || all) {
			due.push_back(rw.get());
		}
	}
	scheduler->clear_targets();
	for (RenderWindow *rw : due) {
		render_one_secondary_window(*rw);
	}
}

void Application::render_one_secondary_window(RenderWindow &rw) {
	const Uint32 width = rw.window->get_width();
	const Uint32 height = rw.window->get_height();
	if (width == 0 || height == 0) {
		return;
	}

	if (m_frame_in_progress) {
		return;
	}
	FrameGuard guard(m_frame_in_progress);

	if (rw.needs_resize || rw.swapchain->needs_resize()) {
		rw.swapchain->resize(width, height);
		rw.needs_resize = false;
		rw.msaa_color.reset(); // force render-target rebuild at the new size
		rw.render_pass.reset();
	}

	const Uint32 target_width = rw.swapchain->get_width();
	const Uint32 target_height = rw.swapchain->get_height();

	if (rw.on_update) {
		rw.on_update(m_frame_delta);
	}
	if (rw.cursor) {
		rw.window->set_cursor(rw.cursor());
	}

	if (!rw.on_render) {
		Uint32 image_index = 0;
		if (!rw.swapchain->acquire_next_image(image_index, false)) {
			return;
		}
		m_ctx->get_device().present_frame(rw.swapchain->get_rhi(), image_index, Vec4(0.F, 0.F, 0.F, 1.F));
		return;
	}

	ensure_window_targets(rw, target_width, target_height);

	Uint32 image_index = 0;
	if (!rw.swapchain->acquire_next_image(image_index, false)) {
		AQUILA_LOG_DEBUG("Secondary swapchain out of date, scheduling another frame");
		Foundation::FrameScheduler::get()->request_frame(rw.window.get());
		return;
	}

	auto cmd = m_ctx->create_command_list(RHI::CommandListType::Graphics, "SecondaryUICmd");
	cmd->begin();

	const Mat4 ortho =
		glm::ortho(0.F, static_cast<float>(target_width), static_cast<float>(target_height), 0.F, -1.F, 1.F);
	rw.render_pass->begin(*cmd, rw.swapchain.get(), image_index);
	m_secondary_batcher->begin(*cmd, RHI::TextureFormat::BGRA8, RHI::SampleCount::X4, ortho);
	rw.on_render(*m_secondary_batcher, *cmd);
	m_secondary_batcher->end();
	rw.render_pass->end(*cmd);

	m_ctx->submit_frame(*cmd, rw.swapchain.get(), image_index);

	if (rw.swapchain->needs_resize()) {
		AQUILA_LOG_DEBUG("Secondary swapchain needs a resize after presenting, scheduling another frame");
		Foundation::FrameScheduler::get()->request_frame(rw.window.get());
	}
}

void Application::run_pre_render(F32 delta_time) {
	Graphics::TextureLibrary::get()->update(*m_ctx);
	on_pre_render(delta_time);
	for (auto &module : m_modules) {
		module->on_pre_render(delta_time);
	}
	m_scene->get_entity_manager()->flush_deletion_queue();
	m_game_input.end_frame();
	if (m_play_state == PlayState::Playing) {
		Foundation::FrameScheduler::get()->request_frame();
	}
}

void Application::internal_update(F32 delta_time) {
	PROFILE_SCOPE("OnUpdate");

	if (m_frame_in_progress) {
		return;
	}

	if (!m_window->is_visible()) {
		{
			FrameGuard guard(m_frame_in_progress);
			run_pre_render(delta_time);
		}
		render_secondary_windows(true);
		return;
	}

	{
		FrameGuard guard(m_frame_in_progress);

		if (m_pending_resize || m_swapchain->needs_resize()) {
			m_pending_resize = false;
			const Uint32 width = m_window->get_width();
			const Uint32 height = m_window->get_height();
			if (width == 0 || height == 0) {
				return;
			}
			handle_resize();
		}

		if (m_render_resize_pending) {
			m_render_resize_pending = false;
			m_ctx->wait_idle();
			m_render_pipeline->resize(m_next_render_width, m_next_render_height);
			m_render_width = m_next_render_width;
			m_render_height = m_next_render_height;
			on_render_resize(m_render_width, m_render_height);
			for (auto &module : m_modules) {
				module->on_render_resize(m_render_width, m_render_height);
			}
		}

		Uint32 image_index = 0;
		{
			PROFILE_SCOPE("AcquireNextImage");
			if (!m_swapchain->acquire_next_image(image_index)) {
				AQUILA_LOG_DEBUG("Main swapchain out of date, scheduling another frame");
				Foundation::FrameScheduler::get()->request_frame();
				return;
			}
		}

		m_renderer->set_swapchain_target(*m_swapchain, image_index);
		m_overlay_renderer->set_swapchain_target(*m_swapchain, image_index);

		Uint32 frame_slot = m_swapchain->get_current_frame_slot();
		auto &cmd = m_ctx->acquire_frame_command_list(frame_slot);
		cmd.begin();

		run_pre_render(delta_time);

		{
			PROFILE_SCOPE("RenderPipeline::Render");
			m_render_pipeline->render(cmd, *m_scene, delta_time);
		}

		{
			PROFILE_SCOPE("SubmitFrame");
			m_ctx->submit_frame(cmd, m_swapchain.get(), image_index);
		}

		if (m_swapchain->needs_resize()) {
			AQUILA_LOG_DEBUG("Main swapchain needs a resize after presenting, scheduling another frame");
			Foundation::FrameScheduler::get()->request_frame();
		}
	}

	render_secondary_windows(true);
}

void Application::internal_on_main_window_event(Platform::Events::Event &event) {
	if (!is_cursor_event(event)) {
		Foundation::FrameScheduler::get()->request_frame();
	}

	on_event(event);
	for (auto &module : m_modules) {
		module->on_event(event);
	}

	Platform::Events::EventDispatcher dispatcher(event);

	dispatcher.dispatch<Platform::Events::WindowCloseEvent>([this](Platform::Events::WindowCloseEvent &) {
		m_running = false;
		return true;
	});

	dispatcher.dispatch<Platform::Events::WindowResizeEvent>([this](Platform::Events::WindowResizeEvent &ev) {
		if (ev.get_width() > 0 && ev.get_height() > 0) {
			m_pending_resize = true;
		}
		return true;
	});
}

void Application::internal_on_secondary_window_event(RenderWindow &rw, Platform::Events::Event &event) {
	if (!is_cursor_event(event)) {
		if (rw.forward_events) {
			Foundation::FrameScheduler::get()->request_frame();
		} else {
			Foundation::FrameScheduler::get()->request_frame(rw.window.get());
		}
	}

	Platform::Events::EventDispatcher dispatcher(event);
	dispatcher.dispatch<Platform::Events::WindowResizeEvent>([&](auto &) {
		rw.needs_resize = true; // swapchain + targets rebuilt in RenderOneSecondaryWindow
		return false;
	});

	if (rw.on_event) {
		rw.on_event(event);
	}

	if (rw.forward_events) {
		for (auto &module : m_modules) {
			module->on_event(event);
		}
	}
}

void Application::request_render_resize(Uint32 width, Uint32 height) {
	if (width == 0 || height == 0) {
		return;
	}
	if (width == m_render_width && height == m_render_height) {
		return;
	}
	m_next_render_width = width;
	m_next_render_height = height;
	m_render_resize_pending = true;
}

void Application::handle_resize() {
	const Uint32 width = m_window->get_width();
	const Uint32 height = m_window->get_height();
	if (width == 0 || height == 0) {
		return;
	}

	m_swapchain->resize(width, height);

	const Uint32 swapchain_width = m_swapchain->get_width();
	const Uint32 swapchain_height = m_swapchain->get_height();

	on_resize(swapchain_width, swapchain_height);
	for (auto &module : m_modules) {
		module->on_resize(swapchain_width, swapchain_height);
	}

	request_render_resize(swapchain_width, swapchain_height);
}

} // namespace Aquila::Application
