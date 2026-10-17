#pragma once

#include "Aquila/Foundation/PrimitiveTypes.h"

template <typename Sig> class Signal;

/// @code
/// Signal<void(Entity)> on_selected;
/// auto connection = on_selected.connect([](Entity entity) { ... });
/// on_selected(entity);
/// on_selected.disconnect(connection);
/// @endcode
///
/// @note Don't connect or disconnect from inside a slot while the signal fires,
/// the slot list can move under the running slot.
template <typename... Args> class Signal<void(Args...)> {
	using Slot = Delegate<void(Args...)>;

  public:
	class Connection {
	  public:
		Connection() = default;
		/// Only tells it came from connect(), it stays true after disconnect().
		[[nodiscard]] bool is_valid() const { return m_id != 0; }

	  private:
		explicit Connection(Uint64 id) : m_id(id) {}
		Uint64 m_id = 0;
		friend class Signal;
	};

	Connection connect(Slot slot) {
		const Uint64 id = ++m_next_id;
		m_slots.push_back({ id, std::move(slot) });
		return Connection(id);
	}

	void disconnect(Connection connection) {
		std::erase_if(m_slots, [id = connection.m_id](const Entry &entry) { return entry.id == id; });
	}

	/// Replaces every connected slot with this one.
	void set(Slot slot) {
		m_slots.clear();
		connect(std::move(slot));
	}

	void clear() { m_slots.clear(); }

	void operator()(Args... args) const {
		for (Usize i = 0; i < m_slots.size(); ++i) {
			m_slots[i].slot(args...);
		}
	}

	[[nodiscard]] bool has_connections() const { return !m_slots.empty(); }

  private:
	struct Entry {
		Uint64 id;
		Slot slot;
	};

	std::vector<Entry> m_slots;
	Uint64 m_next_id = 0;
};
