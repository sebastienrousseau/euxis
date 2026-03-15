#compdef euxis
# Zsh completion for euxis CLI
# Add to fpath or source directly:
#   fpath=(~/.euxis/data/config/completions $fpath)

_euxis() {
    local -a core_cmds system_cmds fleet_cmds aliases

    core_cmds=(
        'check:Verify a repository or target'
        'triage:Fast bounded triage scan'
        'review:Deep verification (standard/forensic)'
        'certify-readiness:Certification readiness assessment'
        'compare:Compare triage vs deep verification'
        'stats:Validation metrics and drift history'
        'policy:Policy inspection and enforcement'
    )

    lifecycle_cmds=(
        'install:Bootstrap local installation'
        'update:Refresh metadata and registry'
        'upgrade:Upgrade binary (pull + rebuild)'
        'uninstall:Remove Euxis from this machine'
        'self:Installation introspection'
    )

    system_cmds=(
        'doctor:Environment diagnostics'
        'fix:Autonomous environment self-repair'
        'health:Fleet integrity check'
        'verify:Verify agent prompt integrity'
        'lint:Lint agent prompts and configs'
    )

    fleet_cmds=(
        'agent:Manage agents'
        'playbook:Execute a playbook manifest'
        'combo:Run agent combo'
        'ci:CI-ready repo verdict'
        'squad:Squad orchestration'
        'dispatch:Dispatch agents from manifest'
        'council:Multi-agent council deliberation'
        'loop:Agent feedback loop'
        'synthesize:Synthesize agent outputs'
    )

    aliases=(
        'quick:Alias for triage'
        'deep:Alias for review'
        'diag:Alias for doctor'
        'metrics:Alias for stats'
        'pb:Alias for playbook'
    )

    if (( CURRENT == 2 )); then
        _describe -t core-commands 'Core commands' core_cmds
        _describe -t lifecycle-commands 'Lifecycle commands' lifecycle_cmds
        _describe -t system-commands 'System commands' system_cmds
        _describe -t fleet-commands 'Fleet commands' fleet_cmds
        _describe -t aliases 'Aliases' aliases
        return
    fi

    case "$words[2]" in
        install)
            _arguments \
                '--dry-run[Show what would be done]' \
                '--force[Overwrite existing config]' \
                '--shell-setup[Configure shell (PATH, completions)]' \
                '--shell[Shell to configure]:shell:(fish bash zsh)' \
                '--no-completions[Skip completions]' \
                '--help[Show help]'
            ;;
        update)
            _arguments \
                '--dry-run[Show what would be done]' \
                '--fetch[Fetch latest git metadata]' \
                '--help[Show help]'
            ;;
        upgrade)
            _arguments \
                '--dry-run[Show what would be done]' \
                '--clean[Clean build directory first]' \
                '--help[Show help]'
            ;;
        uninstall)
            _arguments \
                '--purge[Remove EUXIS_HOME and all data]' \
                '--dry-run[Show what would be done]' \
                '--yes[Skip confirmation]' \
                '--help[Show help]'
            ;;
        self)
            local -a subcmds
            subcmds=(
                'status:Show installation status'
                'paths:Print all known paths'
                'doctor:Run diagnostics'
                'version:Show version info'
                'where:Print binary path'
                'repair:Attempt automatic repair'
            )
            if (( CURRENT == 3 )); then
                _describe 'subcommand' subcmds
            fi
            ;;
        check)
            _arguments \
                '--triage[Run fast triage mode]' \
                '--standard[Force standard mode]' \
                '--forensic[Run forensic verification]' \
                '--policy[Apply policy evaluation]:policy file:_files -g "*.json"' \
                '--ci[CI-safe output]' \
                '--json[JSON output]' \
                '--help[Show help]' \
                '*:target:_files -/'
            ;;
        triage|quick)
            _arguments \
                '--policy[Apply policy evaluation]:policy file:_files -g "*.json"' \
                '--ci[CI-safe output]' \
                '--json[JSON output]' \
                '--help[Show help]' \
                '*:target:_files -/'
            ;;
        review|deep)
            _arguments \
                '--forensic[Run forensic verification]' \
                '--policy[Apply policy evaluation]:policy file:_files -g "*.json"' \
                '--ci[CI-safe output]' \
                '--json[JSON output]' \
                '--help[Show help]' \
                '*:target:_files -/'
            ;;
        compare)
            _arguments \
                '--json[JSON output]' \
                '--policy[Apply policy evaluation]:policy file:_files -g "*.json"' \
                '--help[Show help]' \
                '*:target:_files -/'
            ;;
        stats|metrics)
            _arguments \
                '--since[Filter by date]:date:' \
                '--last[Last N runs]:count:' \
                '--check-baseline[Exit non-zero if targets not met]' \
                '--json[JSON output]' \
                '--help[Show help]'
            ;;
        policy)
            local -a subcmds
            subcmds=(
                'check:Evaluate policy against verdict'
                'show:Display active policy'
                'validate:Validate policy JSON'
            )
            if (( CURRENT == 3 )); then
                _describe 'subcommand' subcmds
            fi
            ;;
        certify-readiness)
            if (( CURRENT == 3 )); then
                local -a subcmds
                subcmds=(
                    'controls:Print domain model'
                    'report:Pretty-print existing artifact'
                )
                _describe 'subcommand' subcmds
            else
                _arguments \
                    '--framework[Framework overlay]:framework:(general soc2 iso27001)' \
                    '--strict[Make soft gates blocking]' \
                    '--ci[CI-safe output]' \
                    '--json[JSON output]' \
                    '--no-build[Skip build gate]' \
                    '--no-tests[Skip test gate]' \
                    '--no-security[Skip security gate]' \
                    '--commit-window[Commits to check]:count:' \
                    '--since-ref[Git ref baseline]:ref:' \
                    '--output[Custom output path]:path:_files' \
                    '--help[Show help]' \
                    '*:target:_files -/'
            fi
            ;;
        playbook|pb)
            _arguments \
                '--mode[Execution mode]:mode:(flash standard forensic)' \
                '--stats[Show metrics]' \
                '--compare[A/B comparison]' \
                '--ci[CI-safe output]' \
                '--json[JSON output]' \
                '--check-baseline[Exit non-zero if targets not met]' \
                '*:manifest:_files -g "*.json"'
            ;;
        agent)
            local -a subcmds
            subcmds=('list' 'info' 'register' 'unregister')
            if (( CURRENT == 3 )); then
                _describe 'subcommand' subcmds
            fi
            ;;
        combo)
            local -a subcmds
            subcmds=('list' 'show' 'run')
            if (( CURRENT == 3 )); then
                _describe 'subcommand' subcmds
            fi
            ;;
    esac
}

_euxis "$@"
