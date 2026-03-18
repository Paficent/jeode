#include "sandbox.h"

#include <spdlog/spdlog.h>
#include <sstream>

// TODO: Use a prexisting LUAC sandbox or create my own once I have more C/Lua functions

Sandbox::Sandbox() {
	block_global("loadfile");
	block_global("dofile");
	block_global("load");

	block_member("os", "execute");
	block_member("os", "remove");
	block_member("os", "rename");
	block_member("os", "exit");
	block_member("os", "tmpname");
}

void Sandbox::block_global(const std::string &name) {
	m_blocked_globals.insert(name);
}

void Sandbox::block_member(const std::string &global, const std::string &member) {
	m_blocked_members[global].insert(member);
}

void Sandbox::set_allow_unsafe(bool allow) {
	m_allow_unsafe = allow;
}

bool Sandbox::is_unsafe_allowed() const {
	return m_allow_unsafe;
}

std::string Sandbox::generate_lua(const std::string &env_var) const {
	std::ostringstream out;

	out << env_var << ".package = nil\n";

	if (m_allow_unsafe) return out.str();

	spdlog::debug("[sandbox] generating sandbox: blocking {} globals, {} member groups", m_blocked_globals.size(),
				  m_blocked_members.size());

	out << "do\n";
	out << "  local function _blocked() error('blocked unsafe function call') "
		   "end\n";
	for (const auto &name : m_blocked_globals) {
		out << "  " << env_var << "['" << name << "'] = _blocked\n";
	}

	for (const auto &[global, members] : m_blocked_members) {
		out << "  do\n";
		out << "    local orig = " << global << "\n";
		out << "    if type(orig) == 'table' then\n";
		out << "      " << env_var << "['" << global << "'] = setmetatable({}, {\n";
		out << "        __index = function(_, k)\n";
		for (const auto &member : members) {
			out << "          if k == '" << member << "' then return _blocked end\n";
		}
		out << "          return orig[k]\n";
		out << "        end,\n";
		out << "        __newindex = orig,\n";
		out << "      })\n";
		out << "    end\n";
		out << "  end\n";
	}

	out << "end\n";

	out << env_var << ".package = nil\n";

	return out.str();
}
