# Bash completion for euxis
# Source this file or add to ~/.bash_completion.d/

_euxis_completions() {
    local cur prev words cword
    _init_completion || return

    local commands="squad combo playbook dispatch council loop verify health certify lint test audit bench cortex graph bus gateway deploy daemon gym kaizen agents search doctor replay"
    local agents="arbiter architect auditor critic guard historian librarian orchestrator pair planner reviewer route debugger tester repairer maintainer optimizer researcher deep-researcher investigator inspector pentester sentinel watchdog gatekeeper cryptographer designer writer animator marketer evangelist automaton butler custodian heal govern trace bridge conduit ambassador interactor responder strategist tactician accountant ledger telemetrist polyglot localizer distill"
    local providers="claude gemini openai ollama qwen crush kiro-cli goose"
    local squads="vision build quality growth experience specialist"
    local combos="envision protect craft refine seal"

    case "$prev" in
        euxis)
            COMPREPLY=($(compgen -W "$commands $agents --help --version" -- "$cur"))
            return
            ;;
        squad)
            COMPREPLY=($(compgen -W "list info deploy" -- "$cur"))
            return
            ;;
        combo)
            COMPREPLY=($(compgen -W "list info run" -- "$cur"))
            return
            ;;
        playbook)
            COMPREPLY=($(compgen -W "list info run" -- "$cur"))
            return
            ;;
        cortex)
            COMPREPLY=($(compgen -W "search prune sync stats" -- "$cur"))
            return
            ;;
        graph)
            COMPREPLY=($(compgen -W "query add visualize" -- "$cur"))
            return
            ;;
        bus)
            COMPREPLY=($(compgen -W "publish subscribe topics" -- "$cur"))
            return
            ;;
        gateway)
            COMPREPLY=($(compgen -W "start stop status" -- "$cur"))
            return
            ;;
        deploy)
            if [[ "${words[1]}" == "squad" ]]; then
                COMPREPLY=($(compgen -W "$squads" -- "$cur"))
            fi
            return
            ;;
        run)
            if [[ "${words[1]}" == "combo" ]]; then
                COMPREPLY=($(compgen -W "$combos" -- "$cur"))
            elif [[ "${words[1]}" == "playbook" ]]; then
                # List playbook files
                local playbooks_dir="${EUXIS_HOME:-$HOME/.euxis}/euxis-core/config/playbooks"
                if [[ -d "$playbooks_dir" ]]; then
                    local playbooks=$(ls "$playbooks_dir"/*.json 2>/dev/null | xargs -I{} basename {} .json)
                    COMPREPLY=($(compgen -W "$playbooks" -- "$cur"))
                fi
            fi
            return
            ;;
        info)
            if [[ "${words[1]}" == "squad" ]]; then
                COMPREPLY=($(compgen -W "$squads" -- "$cur"))
            elif [[ "${words[1]}" == "combo" ]]; then
                COMPREPLY=($(compgen -W "$combos" -- "$cur"))
            fi
            return
            ;;
        --provider)
            COMPREPLY=($(compgen -W "$providers" -- "$cur"))
            return
            ;;
        search)
            COMPREPLY=($(compgen -W "security testing design research operations integration analytics code performance" -- "$cur"))
            return
            ;;
    esac

    # Global options
    if [[ "$cur" == -* ]]; then
        COMPREPLY=($(compgen -W "--provider --json --verbose --no-color --help" -- "$cur"))
        return
    fi
}

complete -F _euxis_completions euxis
