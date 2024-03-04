import logging
import asyncio
from commands.commands import Command

from prompt_toolkit.formatted_text import FormattedText
from prompt_toolkit.styles import Style
from prompt_toolkit.completion import WordCompleter, Completer
from prompt_toolkit.contrib.completers.system import SystemCompleter
from prompt_toolkit import print_formatted_text

from typing import Tuple

# Get parent logger
logger = logging.getLogger(__name__)

def initial_msg() -> str:
    return """\

            ██████╗  █████╗ ████████╗ █████╗     ██╗      ██████╗  ██████╗  ██████╗ ███████╗██████╗ 
            ██╔══██╗██╔══██╗╚══██╔══╝██╔══██╗    ██║     ██╔═══██╗██╔════╝ ██╔════╝ ██╔════╝██╔══██╗
            ██║  ██║███████║   ██║   ███████║    ██║     ██║   ██║██║  ███╗██║  ███╗█████╗  ██████╔╝
            ██║  ██║██╔══██║   ██║   ██╔══██║    ██║     ██║   ██║██║   ██║██║   ██║██╔══╝  ██╔══██╗
            ██████╔╝██║  ██║   ██║   ██║  ██║    ███████╗╚█████╔╝╚██████╔╝╚██████╔╝███████╗██║  ██║
            ╚═════╝ ╚═╝  ╚═╝   ╚═╝   ╚═╝  ╚═╝    ╚══════╝ ╚═════╝  ╚═════╝  ╚═════╝ ╚══════╝╚═╝  ╚═╝
                                                                                                    
    """

def formatted_initial_msg() -> FormattedText:
    return FormattedText([("ansiyellow", f"{initial_msg()}")])

def print_formatted_initial_msg() -> None:
    print_formatted_text(formatted_initial_msg())

# Function to get the prompt toolkit style
def prompt_toolkit_style() -> Style:
    return Style.from_dict({
    # User input (default text).
       '':          'ansidefault', # ANSI default foreground colour
    
       # Prompt
       'interface': 'ansidefault',
       'forward_arrow': 'ansidefault',  # ANSI default foreground colour
    
       # Toolbar
       'bottom-toolbar': 'ansibrightyellow bg:ansidefault',
       'toolbar_name': ' bg:ansibrightyellow bg:ansidefault',
       'mode': 'ansibrightyellow bg:ansiblue',
       'separator' : 'ansibrightyellow',
       'configured': f"{'ansibrightyellow bg:ansigreen' if Command.configured else 'ansibrightyellow bg:ansibrightred'}"
    })

# Function to return prompt message
def prompt_msg() -> list[Tuple]:
    prompt_msg = [
        ("class:interface", f"{Command.interface}"),
        ("class:forward_arrow", f"> "),
    ]
    return prompt_msg
# Function to return the toolbar
def toolbar_text() -> list[Tuple]:
    mode = "Streaming" if Command.streaming else "Message"
    configured = "Configured" if Command.configured else "Not Configured"
    toolbar_text = [
            ("class:bottom-toolbar",""),
            ("class:toolbar_name", "Data Logger "),
            ("class:mode", f" [{mode}]"),
            ("class:separator", f"  "),
            ("class:configured", f"[{configured}]")
    ]
    return toolbar_text

# Combined completer for tab autocompletion for the prompt session, it works out which completer to use based on current user input
class CombinedCompleter(Completer):
    def __init__(self, word_completer: WordCompleter):
        self.word_completer = word_completer
        # System completer, it works better than Path completer as you can have space in the current input and it would still work
        self.system_completer = SystemCompleter()

    def get_completions(self, document, complete_event):
        # Check if the cursor is before the first space in the line, only complete with word completer for the command
        if ' ' not in document.text_before_cursor:
            return self.word_completer.get_completions(document, complete_event)

        # Check if the previous word is a path
        words_before_cursor = document.text_before_cursor.split(' ')
        # logger.debug(f"last_word : {words_before_cursor}")
        if '/' in words_before_cursor[-1]:
            # Use SystemCompleter for subsequent words
            return self.system_completer.get_completions(document, complete_event)

        # Use AutoSuggestFromHistory for subsequent words
        return []
