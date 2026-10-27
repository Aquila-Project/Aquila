#ifndef AQUILA_UI_CORE_COMPONENT_INSTANCE_H
#define AQUILA_UI_CORE_COMPONENT_INSTANCE_H

#include "Aquila/Foundation/Macros.h"
#include "Aquila/Foundation/PrimitiveTypes.h"

namespace Aquila::UI::Markup {
struct MarkupDocument;
}

namespace Aquila::UI::Core {

class Behaviour;
class LayoutLoader;
class View;

class ComponentInstance {
  public:
	ComponentInstance();
	~ComponentInstance();

	AQUILA_NONCOPYABLE(ComponentInstance);
	AQUILA_NONMOVEABLE(ComponentInstance);

	[[nodiscard]] Uint64 get_scope_id() const { return m_scope_id; }
	[[nodiscard]] const std::string &get_name() const { return m_name; }
	[[nodiscard]] const std::string &get_file() const { return m_file; }
	[[nodiscard]] Behaviour *get_behaviour() const { return m_behaviour.get(); }
	[[nodiscard]] ComponentInstance *get_inner() const { return m_inner.get(); }

  private:
	friend class Behaviour;
	friend class LayoutLoader;

	Uint64 m_scope_id = 0;
	std::string m_name;
	std::string m_file;
	Ref<const Markup::MarkupDocument> m_document;
	std::map<std::string, std::string, std::less<>> m_attributes;
	View *m_root = nullptr;
	LayoutLoader *m_loader = nullptr;
	Unique<ComponentInstance> m_inner;
	Unique<Behaviour> m_behaviour;
};

}

#endif
