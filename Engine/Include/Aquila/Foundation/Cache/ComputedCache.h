#pragma once

namespace Aquila::Foundation {

template <typename Key, typename Value, typename Hash = std::hash<Key>> class ComputedCache {
  public:
	template <typename ComputeFn> const Value &get(const Key &key, ComputeFn &&compute_fn) {
		if (auto it = m_entries.find(key); it != m_entries.end() && it->second.valid) {
			return it->second.value;
		}
		Value computed = compute_fn();
		auto &entry = m_entries[key]; // fresh lookup after potential rehash
		entry.value = std::move(computed);
		entry.valid = true;
		return entry.value;
	}

	[[nodiscard]] const Value *peek(const Key &key) const {
		auto it = m_entries.find(key);
		if (it != m_entries.end() && it->second.valid) {
			return &it->second.value;
		}
		return nullptr;
	}

	// Mark key invalid and cascade to all keys that declared a dependency on it.
	void invalidate(const Key &key) {
		auto it = m_entries.find(key);
		if (it == m_entries.end() || !it->second.valid) {
			return;
		}
		it->second.valid = false;

		auto deps_it = m_dependents.find(key);
		if (deps_it != m_dependents.end()) {
			for (const Key &dep : deps_it->second) {
				invalidate(dep);
			}
		}
	}

	void invalidate_all() {
		for (auto &[key, entry] : m_entries) {
			entry.valid = false;
		}
	}

	void register_dependency(const Key &dependent, const Key &dependency) {
		auto &list = m_dependents[dependency];
		for (const Key &k : list) {
			if (k == dependent) {
				return;
			}
		}
		list.push_back(dependent);
	}

	void remove(const Key &key) {
		m_entries.erase(key);
		m_dependents.erase(key); // remove key as a dependency source
	}

	[[nodiscard]] bool is_valid(const Key &key) const {
		auto it = m_entries.find(key);
		return it != m_entries.end() && it->second.valid;
	}

  private:
	struct Entry {
		Value value{};
		bool valid = false;
	};
	std::unordered_map<Key, Entry, Hash> m_entries;
	std::unordered_map<Key, std::vector<Key>, Hash> m_dependents;
};

} // namespace Aquila::Foundation
