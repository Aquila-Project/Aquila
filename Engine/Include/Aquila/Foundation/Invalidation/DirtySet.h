#pragma once

namespace Aquila::Foundation {

template <typename Key, typename Hash = std::hash<Key>> class DirtySet {
  public:
	void mark_dirty(const Key &key) {
		if (m_set.insert(key).second) {
			m_ordered.push_back(key);
		}
	}

	[[nodiscard]] bool is_dirty(const Key &key) const { return m_set.count(key) > 0; }
	[[nodiscard]] bool is_empty() const { return m_ordered.empty(); }
	[[nodiscard]] const std::vector<Key> &get_ordered() const { return m_ordered; }

	void remove(const Key &key) {
		if (m_set.erase(key)) {
			std::erase(m_ordered, key);
		}
	}

	void clear() {
		m_set.clear();
		m_ordered.clear();
	}

  private:
	std::unordered_set<Key, Hash> m_set;
	std::vector<Key> m_ordered;
};

} // namespace Aquila::Foundation
