#pragma once

#include <string>
#include <unordered_map>
#include <unordered_set>

class Sandbox {
  public:
	Sandbox();

	void block_global(const std::string &name);
	void block_member(const std::string &global, const std::string &member);

	void set_allow_unsafe(bool allow);
	bool is_unsafe_allowed() const;

	// Returns a Lua code fragment that applies sandbox restrictions.
	// The generated code expects `env_var` to be a local in scope.
	std::string generate_lua(const std::string &env_var) const;

  private:
	bool m_allow_unsafe = false;
	std::unordered_set<std::string> m_blocked_globals;
	std::unordered_map<std::string, std::unordered_set<std::string>> m_blocked_members;
};
