/*
 * Copyright (c) [2021-2026] SUSE LLC
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


#include <storage/Storage.h>
#include <storage/Actiongraph.h>
#include <storage/Actions/Base.h>
#include <storage/Actions/Create.h>
#include <storage/Actions/Delete.h>

#include "commit.h"
#include "Utils/Text.h"
#include "Utils/Prompt.h"
#include "Utils/Colors.h"
#include "show-commit.h"
#include "save-pools.h"


namespace barrel
{

    class ParsedCmdCommit : public ParsedCmd
    {
    public:

	virtual bool do_backup() const override { return false; } // TODO ???

	virtual void doit(const GlobalOptions& global_options, State& state) const override;

    };


    class MyCommitCallbacks : public CommitCallbacksV2
    {
    public:

	MyCommitCallbacks(const Actiongraph* actiongraph)
	    : actiongraph(actiongraph) {}

	void begin_action(const Action::Base* action) const override
	{
	    color = get_color(actiongraph, action);
	}

	void message(const string& message) const override;

	bool error(const std::string& message, const std::string& what) const override;

    private:

	const Actiongraph* actiongraph;

	mutable Color color = Color::BLACK;

    };


    void
    MyCommitCallbacks::message(const string& message) const
    {
	cout << "  " << colorize_message(message, color) << '\n';
    }


    bool
    MyCommitCallbacks::error(const std::string& message, const std::string& what) const
    {
	if (what.empty())
	    cerr << "  " << message << '\n';
	else
	    cerr << "  " << message << " " << what << '\n';

	return false;
    }


    void
    ParsedCmdCommit::doit(const GlobalOptions& global_options, State& state) const
    {
	CmdShowCommit::parse()->doit(global_options, state);

	if (!global_options.yes)
	{
	    if (!prompt(_("Commit changes?")))
		return;
	}

	const Actiongraph* actiongraph = state.storage->calculate_actiongraph();

	if (state.testsuite)
	{
	    for (const Action::Base* action : actiongraph->get_commit_actions())
		state.testsuite->actions.push_back(get_string(actiongraph, action));
	}

	if (state.global_options.dry_run)
	{
	    cout << "dry run" << endl;
	}
	else
	{
	    CommitOptions commit_options(false);
	    MyCommitCallbacks my_commit_callbacks(actiongraph);
	    state.storage->commit(commit_options, &my_commit_callbacks);

	    if (state.pools_modified)
		CmdSavePools::parse()->doit(global_options, state);
	}

	state.run = false;
	state.modified = false;
    }


    shared_ptr<ParsedCmd>
    CmdCommit::parse()
    {
	return make_shared<ParsedCmdCommit>();
    }


    shared_ptr<ParsedCmd>
    CmdCommit::parse(GetOpts& get_opts) const
    {
	get_opts.parse("commit", GetOpts::no_ext_options);

	return make_shared<ParsedCmdCommit>();
    }


    const char*
    CmdCommit::help() const
    {
	return _("Commits changes to disk and quits barrel.");
    }

}
