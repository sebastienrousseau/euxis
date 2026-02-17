#compdef euxis

# Zsh completion for euxis
# Install: cp euxis.zsh ~/.zsh/completions/_euxis

_euxis_agents() {
    local agents=(
        'arbiter:Resolves conflicts between agents'
        'architect:Designs systems and creates manifests'
        'auditor:Reviews code quality and compliance'
        'critic:Provides constructive feedback'
        'guard:Security gatekeeper and validator'
        'historian:Maintains project history and context'
        'librarian:Manages documentation and knowledge'
        'orchestrator:Coordinates multi-agent workflows'
        'pair:Collaborative coding partner'
        'planner:Creates project plans and roadmaps'
        'reviewer:Code review and approval'
        'route:Intelligent task routing'
        'debugger:Finds and fixes bugs'
        'tester:Writes and runs tests'
        'repairer:Fixes broken code and configs'
        'maintainer:Handles upgrades and refactoring'
        'optimizer:Improves performance'
        'researcher:Gathers information and context'
        'deep-researcher:Deep-dive analysis'
        'investigator:Root cause analysis'
        'inspector:Code inspection and review'
        'pentester:Penetration testing'
        'sentinel:Threat detection'
        'watchdog:Continuous monitoring'
        'gatekeeper:Access control'
        'cryptographer:Cryptography specialist'
        'designer:UI/UX design'
        'writer:Technical writing'
        'animator:Motion and interaction'
        'marketer:Marketing content'
        'evangelist:Developer advocacy'
        'automaton:Task automation'
        'butler:Service orchestration'
        'custodian:Resource management'
        'heal:Self-healing operations'
        'govern:Policy enforcement'
        'trace:Distributed tracing'
        'bridge:System integration'
        'conduit:Data pipelines'
        'ambassador:External communications'
        'interactor:User interaction'
        'responder:Event handling'
        'strategist:Strategic planning'
        'tactician:Tactical execution'
        'accountant:Cost analysis'
        'ledger:Audit trails'
        'telemetrist:Metrics collection'
        'polyglot:Multi-language support'
        'localizer:Internationalization'
        'distill:Content summarization'
    )
    _describe -t agents 'agent' agents
}

_euxis_providers() {
    local providers=(
        'claude:Anthropic Claude'
        'gemini:Google Gemini'
        'openai:OpenAI GPT'
        'ollama:Ollama local models'
        'qwen:Alibaba Qwen'
        'crush:Crush AI'
        'kiro-cli:Kiro CLI'
        'goose:Goose AI'
    )
    _describe -t providers 'provider' providers
}

_euxis_squads() {
    local squads=(
        'vision:Strategy, Research, Architecture'
        'build:Engineering & Execution'
        'quality:Assurance & Security'
        'growth:Branding & Documentation'
        'experience:UI Excellence'
        'specialist:Domain-Specific'
    )
    _describe -t squads 'squad' squads
}

_euxis_combos() {
    local combos=(
        'envision:Research to review chain'
        'protect:Security hardening chain'
        'craft:Content creation chain'
        'refine:Design iteration chain'
        'seal:Crypto audit chain'
    )
    _describe -t combos 'combo' combos
}

_euxis_playbooks() {
    local playbooks_dir="${EUXIS_HOME:-$HOME/.euxis}/euxis-core/config/playbooks"
    if [[ -d "$playbooks_dir" ]]; then
        local playbooks=(${(f)"$(ls "$playbooks_dir"/*.json 2>/dev/null | xargs -I{} basename {} .json)"})
        _describe -t playbooks 'playbook' playbooks
    fi
}

_euxis() {
    local context state state_descr line
    typeset -A opt_args

    _arguments -C \
        '1: :->command' \
        '*:: :->args' \
        '--provider[Override AI provider]:provider:_euxis_providers' \
        '--json[Output as JSON]' \
        '--verbose[Enable verbose logging]' \
        '--no-color[Disable colored output]' \
        '--help[Show help]' \
        '--version[Show version]'

    case $state in
        command)
            local commands=(
                'squad:Manage agent squads'
                'combo:Chain agents sequentially'
                'playbook:Phased execution with gates'
                'dispatch:Deploy fleet from manifest'
                'council:Multi-agent debate'
                'loop:Autonomous retry loop'
                'verify:Pre-commit quality gates'
                'health:System health check'
                'certify:Certification pipeline'
                'lint:Static analysis'
                'test:Unit tests'
                'audit:Security audit'
                'bench:Performance benchmarks'
                'cortex:Vector memory'
                'graph:Knowledge graph'
                'bus:Message bus'
                'gateway:Gateway control'
                'deploy:Docker deployment'
                'daemon:Kaizen daemon'
                'gym:Agent evaluation'
                'kaizen:Self-improvement'
                'doctor:Verify installation'
                'agents:List all agents'
                'search:Find agents by capability'
                'replay:Replay recorded traces'
            )
            _describe -t commands 'command' commands
            _euxis_agents
            ;;
        args)
            case $line[1] in
                squad)
                    _arguments \
                        '1: :(list info deploy)' \
                        '2: :_euxis_squads'
                    ;;
                combo)
                    _arguments \
                        '1: :(list info run)' \
                        '2: :_euxis_combos'
                    ;;
                playbook)
                    _arguments \
                        '1: :(list info run)' \
                        '2: :_euxis_playbooks'
                    ;;
                cortex)
                    _arguments '1: :(search prune sync stats)'
                    ;;
                graph)
                    _arguments '1: :(query add visualize)'
                    ;;
                bus)
                    _arguments '1: :(publish subscribe topics)'
                    ;;
                gateway)
                    _arguments '1: :(start stop status)'
                    ;;
                search)
                    local categories=(security testing design research operations integration analytics code performance)
                    _describe -t categories 'category' categories
                    ;;
                *)
                    _euxis_providers
                    ;;
            esac
            ;;
    esac
}

_euxis "$@"
