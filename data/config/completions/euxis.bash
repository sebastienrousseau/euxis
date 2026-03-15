# Bash completion for euxis CLI
# Source this file or add to ~/.bashrc:
#   source ~/.euxis/data/config/completions/euxis.bash

_euxis_completions() {
    local cur prev
    cur="${COMP_WORDS[COMP_CWORD]}"
    prev="${COMP_WORDS[COMP_CWORD-1]}"

    # Core commands (user-facing)
    local core_cmds="check triage review certify-readiness compare stats policy"
    # Lifecycle commands
    local lifecycle_cmds="install update upgrade uninstall self"
    # System commands
    local system_cmds="doctor fix health verify lint shell-lint verify-all cross-platform-verify"
    # Fleet commands
    local fleet_cmds="agent agent-bootstrap squad combo playbook ci dispatch council loop synthesize"
    # Other commands
    local other_cmds="cortex graph codex omnigraph slash gateway bus daemon deploy optimize bench hooks setup-shell-hooks git-guard license-check docs-test sync-docs test-infra voice tui gui polish kaizen audit audit-run certify evidence-verify gym replay context-worker"
    # Aliases
    local aliases="quick deep diag metrics pb"

    local all_cmds="$core_cmds $lifecycle_cmds $system_cmds $fleet_cmds $other_cmds $aliases"

    # Global flags
    local global_flags="--help --version --json --no-color --verbose"

    case "$prev" in
        euxis)
            COMPREPLY=($(compgen -W "$all_cmds $global_flags" -- "$cur"))
            return 0
            ;;
        check)
            COMPREPLY=($(compgen -W "--triage --standard --forensic --policy --ci --json --help" -- "$cur"))
            return 0
            ;;
        triage|quick)
            COMPREPLY=($(compgen -W "--policy --ci --json --help" -- "$cur"))
            return 0
            ;;
        review|deep)
            COMPREPLY=($(compgen -W "--forensic --policy --ci --json --help" -- "$cur"))
            return 0
            ;;
        compare)
            COMPREPLY=($(compgen -W "--json --policy --help" -- "$cur"))
            return 0
            ;;
        stats|metrics)
            COMPREPLY=($(compgen -W "--since --last --check-baseline --json --help" -- "$cur"))
            return 0
            ;;
        policy)
            COMPREPLY=($(compgen -W "check show validate --help" -- "$cur"))
            return 0
            ;;
        certify-readiness)
            COMPREPLY=($(compgen -W "controls report --framework --strict --ci --json --no-build --no-tests --no-security --commit-window --since-ref --output --help" -- "$cur"))
            return 0
            ;;
        install)
            COMPREPLY=($(compgen -W "--dry-run --force --shell-setup --shell --no-completions --help" -- "$cur"))
            return 0
            ;;
        update)
            COMPREPLY=($(compgen -W "--dry-run --fetch --help" -- "$cur"))
            return 0
            ;;
        upgrade)
            COMPREPLY=($(compgen -W "--dry-run --clean --help" -- "$cur"))
            return 0
            ;;
        uninstall)
            COMPREPLY=($(compgen -W "--purge --dry-run --yes --help" -- "$cur"))
            return 0
            ;;
        self)
            COMPREPLY=($(compgen -W "status paths doctor version where repair --help" -- "$cur"))
            return 0
            ;;
        doctor|diag)
            COMPREPLY=($(compgen -W "--providers --routing --registry --json --help" -- "$cur"))
            return 0
            ;;
        playbook|pb)
            COMPREPLY=($(compgen -W "verify-everything --stats --compare --mode --ci --json --help" -- "$cur"))
            return 0
            ;;
        agent)
            COMPREPLY=($(compgen -W "list info register unregister --help" -- "$cur"))
            return 0
            ;;
        combo)
            COMPREPLY=($(compgen -W "list show run --help" -- "$cur"))
            return 0
            ;;
        --mode)
            COMPREPLY=($(compgen -W "flash standard forensic" -- "$cur"))
            return 0
            ;;
        --since)
            # Date completion hint
            COMPREPLY=()
            return 0
            ;;
        --last)
            COMPREPLY=($(compgen -W "5 10 20 50" -- "$cur"))
            return 0
            ;;
    esac

    # Default: file/directory completion
    COMPREPLY=($(compgen -f -- "$cur"))
    return 0
}

complete -o default -F _euxis_completions euxis
