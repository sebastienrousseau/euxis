# Fish completion for euxis CLI
# Copy to ~/.config/fish/completions/euxis.fish
# Or symlink: ln -s ~/.euxis/data/config/completions/euxis.fish ~/.config/fish/completions/

# Disable file completions by default for subcommands
complete -c euxis -f

# Core commands
complete -c euxis -n '__fish_use_subcommand' -a check -d 'Verify a repository or target'
complete -c euxis -n '__fish_use_subcommand' -a triage -d 'Fast bounded triage scan'
complete -c euxis -n '__fish_use_subcommand' -a review -d 'Deep verification (standard/forensic)'
complete -c euxis -n '__fish_use_subcommand' -a compare -d 'Compare triage vs deep verification'
complete -c euxis -n '__fish_use_subcommand' -a stats -d 'Validation metrics and drift history'
complete -c euxis -n '__fish_use_subcommand' -a policy -d 'Policy inspection and enforcement'
complete -c euxis -n '__fish_use_subcommand' -a certify-readiness -d 'Certification readiness assessment'

# Lifecycle commands
complete -c euxis -n '__fish_use_subcommand' -a install -d 'Bootstrap local installation'
complete -c euxis -n '__fish_use_subcommand' -a update -d 'Refresh metadata and registry'
complete -c euxis -n '__fish_use_subcommand' -a upgrade -d 'Upgrade binary (pull + rebuild)'
complete -c euxis -n '__fish_use_subcommand' -a uninstall -d 'Remove Euxis from this machine'
complete -c euxis -n '__fish_use_subcommand' -a self -d 'Installation introspection'

# System commands
complete -c euxis -n '__fish_use_subcommand' -a doctor -d 'Environment diagnostics'
complete -c euxis -n '__fish_use_subcommand' -a health -d 'Fleet integrity check'
complete -c euxis -n '__fish_use_subcommand' -a verify -d 'Verify agent prompt integrity'
complete -c euxis -n '__fish_use_subcommand' -a lint -d 'Lint agent prompts and configs'

# Fleet commands
complete -c euxis -n '__fish_use_subcommand' -a agent -d 'Manage agents'
complete -c euxis -n '__fish_use_subcommand' -a playbook -d 'Execute a playbook manifest'
complete -c euxis -n '__fish_use_subcommand' -a combo -d 'Run agent combo'
complete -c euxis -n '__fish_use_subcommand' -a ci -d 'CI-ready repo verdict'
complete -c euxis -n '__fish_use_subcommand' -a squad -d 'Squad orchestration'

# Aliases
complete -c euxis -n '__fish_use_subcommand' -a quick -d 'Alias for triage'
complete -c euxis -n '__fish_use_subcommand' -a deep -d 'Alias for review'
complete -c euxis -n '__fish_use_subcommand' -a diag -d 'Alias for doctor'
complete -c euxis -n '__fish_use_subcommand' -a metrics -d 'Alias for stats'
complete -c euxis -n '__fish_use_subcommand' -a pb -d 'Alias for playbook'

# Global flags
complete -c euxis -n '__fish_use_subcommand' -l help -d 'Show help'
complete -c euxis -n '__fish_use_subcommand' -l version -d 'Show version'
complete -c euxis -n '__fish_use_subcommand' -l json -d 'JSON output'
complete -c euxis -n '__fish_use_subcommand' -l no-color -d 'Disable colors'
complete -c euxis -n '__fish_use_subcommand' -l verbose -d 'Verbose logging'

# check flags
complete -c euxis -n '__fish_seen_subcommand_from check' -l triage -d 'Run fast triage mode'
complete -c euxis -n '__fish_seen_subcommand_from check' -l standard -d 'Force standard mode'
complete -c euxis -n '__fish_seen_subcommand_from check' -l forensic -d 'Run forensic verification'
complete -c euxis -n '__fish_seen_subcommand_from check' -l policy -d 'Apply policy evaluation'
complete -c euxis -n '__fish_seen_subcommand_from check' -l ci -d 'CI-safe output'
complete -c euxis -n '__fish_seen_subcommand_from check' -l json -d 'JSON output'
complete -c euxis -n '__fish_seen_subcommand_from check' -F  # allow file completion for target

# triage flags
complete -c euxis -n '__fish_seen_subcommand_from triage quick' -l policy -d 'Apply policy evaluation'
complete -c euxis -n '__fish_seen_subcommand_from triage quick' -l ci -d 'CI-safe output'
complete -c euxis -n '__fish_seen_subcommand_from triage quick' -l json -d 'JSON output'
complete -c euxis -n '__fish_seen_subcommand_from triage quick' -F

# review flags
complete -c euxis -n '__fish_seen_subcommand_from review deep' -l forensic -d 'Run forensic verification'
complete -c euxis -n '__fish_seen_subcommand_from review deep' -l policy -d 'Apply policy evaluation'
complete -c euxis -n '__fish_seen_subcommand_from review deep' -l ci -d 'CI-safe output'
complete -c euxis -n '__fish_seen_subcommand_from review deep' -l json -d 'JSON output'
complete -c euxis -n '__fish_seen_subcommand_from review deep' -F

# compare flags
complete -c euxis -n '__fish_seen_subcommand_from compare' -l json -d 'JSON output'
complete -c euxis -n '__fish_seen_subcommand_from compare' -l policy -d 'Apply policy evaluation'
complete -c euxis -n '__fish_seen_subcommand_from compare' -F

# stats flags
complete -c euxis -n '__fish_seen_subcommand_from stats metrics' -l since -d 'Filter by date (YYYY-MM-DD)' -x
complete -c euxis -n '__fish_seen_subcommand_from stats metrics' -l last -d 'Last N runs' -x
complete -c euxis -n '__fish_seen_subcommand_from stats metrics' -l check-baseline -d 'Exit non-zero if targets not met'
complete -c euxis -n '__fish_seen_subcommand_from stats metrics' -l json -d 'JSON output'

# policy subcommands
complete -c euxis -n '__fish_seen_subcommand_from policy' -a check -d 'Evaluate policy against verdict'
complete -c euxis -n '__fish_seen_subcommand_from policy' -a show -d 'Display active policy'
complete -c euxis -n '__fish_seen_subcommand_from policy' -a validate -d 'Validate policy JSON'

# certify-readiness subcommands and flags
complete -c euxis -n '__fish_seen_subcommand_from certify-readiness' -a controls -d 'Print domain model'
complete -c euxis -n '__fish_seen_subcommand_from certify-readiness' -a report -d 'Pretty-print artifact'
complete -c euxis -n '__fish_seen_subcommand_from certify-readiness' -l framework -d 'Framework overlay' -x -a 'general soc2 iso27001'
complete -c euxis -n '__fish_seen_subcommand_from certify-readiness' -l strict -d 'Make soft gates blocking'
complete -c euxis -n '__fish_seen_subcommand_from certify-readiness' -l ci -d 'CI-safe output'
complete -c euxis -n '__fish_seen_subcommand_from certify-readiness' -l json -d 'JSON output'
complete -c euxis -n '__fish_seen_subcommand_from certify-readiness' -l no-build -d 'Skip build gate'
complete -c euxis -n '__fish_seen_subcommand_from certify-readiness' -l no-tests -d 'Skip test gate'
complete -c euxis -n '__fish_seen_subcommand_from certify-readiness' -l no-security -d 'Skip security gate'
complete -c euxis -n '__fish_seen_subcommand_from certify-readiness' -l commit-window -d 'Commits to check' -x
complete -c euxis -n '__fish_seen_subcommand_from certify-readiness' -l since-ref -d 'Git ref baseline' -x
complete -c euxis -n '__fish_seen_subcommand_from certify-readiness' -l output -d 'Custom output path' -r
complete -c euxis -n '__fish_seen_subcommand_from certify-readiness' -F

# playbook flags
complete -c euxis -n '__fish_seen_subcommand_from playbook pb' -l mode -d 'Execution mode' -x -a 'flash standard forensic'
complete -c euxis -n '__fish_seen_subcommand_from playbook pb' -l stats -d 'Show metrics'
complete -c euxis -n '__fish_seen_subcommand_from playbook pb' -l compare -d 'A/B comparison'
complete -c euxis -n '__fish_seen_subcommand_from playbook pb' -l ci -d 'CI-safe output'
complete -c euxis -n '__fish_seen_subcommand_from playbook pb' -l json -d 'JSON output'
complete -c euxis -n '__fish_seen_subcommand_from playbook pb' -a 'verify-everything' -d 'Full verification playbook'

# agent subcommands
complete -c euxis -n '__fish_seen_subcommand_from agent' -a list -d 'List agents'
complete -c euxis -n '__fish_seen_subcommand_from agent' -a info -d 'Agent details'
complete -c euxis -n '__fish_seen_subcommand_from agent' -a register -d 'Register agent'
complete -c euxis -n '__fish_seen_subcommand_from agent' -a unregister -d 'Unregister agent'

# combo subcommands
complete -c euxis -n '__fish_seen_subcommand_from combo' -a list -d 'List combos'
complete -c euxis -n '__fish_seen_subcommand_from combo' -a show -d 'Show combo details'
complete -c euxis -n '__fish_seen_subcommand_from combo' -a run -d 'Run a combo'

# install flags
complete -c euxis -n '__fish_seen_subcommand_from install' -l dry-run -d 'Show what would be done'
complete -c euxis -n '__fish_seen_subcommand_from install' -l force -d 'Overwrite existing config'
complete -c euxis -n '__fish_seen_subcommand_from install' -l shell-setup -d 'Configure shell (PATH, completions)'
complete -c euxis -n '__fish_seen_subcommand_from install' -l shell -d 'Shell to configure' -x -a 'fish bash zsh'
complete -c euxis -n '__fish_seen_subcommand_from install' -l no-completions -d 'Skip completions'

# update flags
complete -c euxis -n '__fish_seen_subcommand_from update' -l dry-run -d 'Show what would be done'
complete -c euxis -n '__fish_seen_subcommand_from update' -l fetch -d 'Fetch latest git metadata'

# upgrade flags
complete -c euxis -n '__fish_seen_subcommand_from upgrade' -l dry-run -d 'Show what would be done'
complete -c euxis -n '__fish_seen_subcommand_from upgrade' -l clean -d 'Clean build directory first'

# uninstall flags
complete -c euxis -n '__fish_seen_subcommand_from uninstall' -l dry-run -d 'Show what would be done'
complete -c euxis -n '__fish_seen_subcommand_from uninstall' -l purge -d 'Remove EUXIS_HOME and all data'
complete -c euxis -n '__fish_seen_subcommand_from uninstall' -l yes -d 'Skip confirmation'

# self subcommands
complete -c euxis -n '__fish_seen_subcommand_from self' -a status -d 'Show installation status'
complete -c euxis -n '__fish_seen_subcommand_from self' -a paths -d 'Print all known paths'
complete -c euxis -n '__fish_seen_subcommand_from self' -a doctor -d 'Run diagnostics'
complete -c euxis -n '__fish_seen_subcommand_from self' -a version -d 'Show version info'
complete -c euxis -n '__fish_seen_subcommand_from self' -a where -d 'Print binary path'
complete -c euxis -n '__fish_seen_subcommand_from self' -a repair -d 'Attempt automatic repair'
