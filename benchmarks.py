from euxis.agent import Agent
from euxis.config import AIConfig, Config
from euxis.main import COMMAND_CATEGORIES
from euxis.memory.vector import get_memory
from euxis.models.command_registry import CommandRegistry
from euxis.prompts.prompt import DEFAULT_TRIGGERING_PROMPT
from euxis.workspace import Workspace


def run_task(task) -> None:
    agent = bootstrap_agent(task)
    agent.start_interaction_loop()


def bootstrap_agent(task):
    config = Config.build_config_from_env()
    config.continuous_mode = False
    config.temperature = 0
    config.plain_output = True
    command_registry = get_command_registry(config)
    config.memory_backend = "no_memory"
    workspace_directory = Workspace.get_workspace_directory(config)
    workspace_directory_path = Workspace.make_workspace(workspace_directory)
    Workspace.build_file_logger_path(config, workspace_directory_path)
    ai_config = AIConfig(
        ai_name="Euxis",
        ai_role="a multi-purpose AI assistant.",
        ai_goals=[task.user_input],
    )
    ai_config.command_registry = command_registry
    system_prompt = ai_config.construct_full_prompt(config)
    return Agent(
        ai_name="Euxis",
        memory=get_memory(config),
        command_registry=command_registry,
        ai_config=ai_config,
        config=config,
        next_action_count=0,
        system_prompt=system_prompt,
        triggering_prompt=DEFAULT_TRIGGERING_PROMPT,
        workspace_directory=str(workspace_directory_path),
    )


def get_command_registry(config):
    command_registry = CommandRegistry()
    enabled_command_categories = [
        x
        for x in COMMAND_CATEGORIES
        if x not in config.disabled_command_categories
    ]
    for command_category in enabled_command_categories:
        command_registry.import_commands(command_category)
    return command_registry
