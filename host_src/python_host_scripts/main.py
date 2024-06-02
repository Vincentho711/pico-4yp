import asyncio
import logging

from prompt_toolkit.shortcuts import PromptSession
from prompt_toolkit.auto_suggest import AutoSuggestFromHistory
from prompt_toolkit.shortcuts import CompleteStyle
from prompt_toolkit.completion import WordCompleter
from prompt_toolkit.patch_stdout import patch_stdout

from commands.commands import Command, CommandFactory
from frontend.cli import CombinedCompleter, print_formatted_initial_msg, prompt_toolkit_style, prompt_msg, toolbar_text

# Define the root logger
logger = logging.getLogger()


async def input_coroutine():
    try:
        # Python toolkit prompt completer
        command_completer = WordCompleter(list(CommandFactory.command_classes.keys()), ignore_case=True)

        # Instantiate CombinedCompleter
        combined_completer = CombinedCompleter(command_completer)

        # Create Prompt session
        prompt_session = PromptSession(
                message = prompt_msg(),
                auto_suggest=AutoSuggestFromHistory(),
                completer=combined_completer,
                complete_style=CompleteStyle.MULTI_COLUMN, # Enable tab completion
                enable_history_search=True,
                bottom_toolbar=toolbar_text(),
                style=prompt_toolkit_style()
                )

        # Print initial message and list of available commands
        print_formatted_initial_msg()
        CommandFactory.print_command_list()

        while True:
            user_input = await prompt_session.prompt_async()
            if user_input:
                logger.debug(user_input)
                # Decode user_input line
                user_input = user_input.lower()
                logger.debug(f"user_input : {user_input}")
                command_name, *command_args = user_input.split()
                logger.debug(f"command_name : {command_name}")
                logger.debug(f"*command_args : {command_args}")

                # Check if it is a help command first
                if user_input.endswith('--help'):
                    if command_name == "--help":
                        CommandFactory.print_command_list()
                    else:
                        parser = CommandFactory.get_command_class(command_name).get_argument_parser()
                        CommandFactory.print_command_help(command_name)
                # Check if it is a valid command
                elif command_name in CommandFactory.command_classes:
                    command_class = CommandFactory.get_command_class(command_name)
                    parser = command_class.get_argument_parser()
                    command_args_namespace = parser.parse_args(command_args)
                    # Execute the command
                    await CommandFactory.execute_command(command_name, command_args_namespace, {})
                else:
                    print(f"Unknown command '{command_name}'. Use '--help' to see a list of valid commands.")
                    logger.error(f"Unknown command '{command_name}'")

                # Set the prompt message to get the updated version
                prompt_session.message = prompt_msg()
                # Set the toolbar to get the updated version
                prompt_session.bottom_toolbar = toolbar_text()
                # Invalidate the prompt_session to show updates
                prompt_session.app.invalidate()

    except (EOFError, KeyboardInterrupt):
        pass

    except Exception as e:
        logger.exception("Exception ocurred in input_coroutine().")

        
async def main():
    try:
        try:
            input_prompt_task = asyncio.create_task(input_coroutine())
            await asyncio.gather(
                input_prompt_task
            )

        except Exception as e:
            logger.exception("Exception occurred in main().")
        finally:
            if Command.async_transport is not None:
                Command.async_transport.close()

    except KeyboardInterrupt:
        pass
    except Exception as e:
        logger.exception("Exception occurred.")

if __name__ == "__main__":
    try:
        # Make sure that the text like print and logging appears above the prompt 
        with patch_stdout():
            # Configure logger, it has to be wrapped within patch_stdout() to ensure the prompt won't dissapear after logger output
            logger_handler = logging.StreamHandler()
            logger_formatter = logging.Formatter('%(asctime)s - %(name)s - %(levelname)s - %(message)s')
            logger_handler.setFormatter(logger_formatter)
            logger.addHandler(logger_handler)
            logger.setLevel(logging.INFO)

            # Start runing the async main()
            asyncio.run(main())
    except KeyboardInterrupt:
        pass
    except Exception as e:
        logger.exception(f"Exception occurred in __main__. : {e}")

