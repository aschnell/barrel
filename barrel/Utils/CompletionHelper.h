/*
 * Copyright (c) 2026 SUSE LLC
 *
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as published
 * by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, contact SUSE LLC.
 *
 * To contact SUSE LLC about this file by physical or electronic mail, you may
 * find current contact information at www.suse.com.
 */


#ifndef BARREL_COMPLETION_PROVIDER_H
#define BARREL_COMPLETION_PROVIDER_H


#include <string>
#include <vector>
#include <map>
#include <ostream>
#include <set>
#include "../cmds.h"
#include "Misc.h"


namespace barrel
{

    class CompletionHelper
    {
    public:

	static string escape(const string& original);
	static string unescape(const string& original);

	enum class Category { NONE, COMMAND, ARGUMENT, POOL, LVM_VG, DEVICE };

	struct CompItem
	{
	    string name;
	    string desc;
	    Category category;
	    string display;         // Used only to display the autocompletions,
				    // but name will be taken. Used for short arguments
	};

	class CompletionResult
	{
	public:

	    bool empty() const { return items.empty(); }

	    void clear();

	    void push_command(const string& name, const string& desc);
	    void push_argument(const string& name, const string& desc, const string& display = "");
	    void push_pool(const string& name);
	    void push_lvm_vg(const string& name);
	    void push_device(const string& name);

	    vector<CompItem> items;

	private:

	    void push(Category category, const string& name, const string& desc = "", const string& display = "");

	};

	void set_storage(const Storage* storage);

	const CompletionResult&
	complete(const vector<string> &tokens, const string &text);

	const CompletionResult&	get_result() const { return result; }

	void display_matches(char** matches, int num_matches, int max_length) const;

    private:

	string removeAfterNthSlash(string str, int n) const;

	const MainCmd* find_main_cmd(const string &name) const;

	pair<const Parser*, size_t>
	find_sub_cmd(const MainCmd* cmd, const vector<string> &items) const;

	const Option* find_option(const Cmd* cmd, const string &text) const;

	void display_comp_items(vector<std::reference_wrapper<const CompItem>> &items) const;

	vector<string> list_files_of_dir(const string &text) const;

	void add_devices(CompletionResult &res, const string &text, const vector<string> &all_devices) const;

	const Storage* storage = nullptr;

	CompletionResult result;

    };

    std::ostream& operator<<(std::ostream& os, CompletionHelper::Category category);

}


#endif
