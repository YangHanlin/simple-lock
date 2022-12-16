import argparse
from curses import meta
from typing import Callable

from . import common, monitor, setup


def _build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser()
    parser.add_argument('--debug', type=lambda choice: choice == 'true', choices=['true', 'false'], help='override debug mode')
    parser.add_argument('--config', metavar='PATH', help='override default config path ' + common.DEFAULT_CONFIG_PATH)
    subparsers = parser.add_subparsers(dest='action_name', metavar='ACTION', required=True)
    subparser_monitor = subparsers.add_parser('monitor', help='start simple-lock daemon')
    subparser_setup = subparsers.add_parser('setup', help='set up simple-lock')
    return parser


def _build_context(args: dict) -> dict:
    config = common.load_config(args.get('config', None))

    context = {}
    for k, v in config.items():
        if v is not None:
            context[k] = v
    for k, v in args.items():
        if v is not None:
            context[k] = v
    
    if context.get('debug', None) is None:
        context['debug'] = False

    return context


def _find_action(action_name: str) -> Callable:
    if action_name == 'monitor':
        return monitor.main
    elif action_name == 'setup':
        return setup.main
    else:
        assert False


def main():
    parser = _build_parser()
    context = _build_context(vars(parser.parse_args()))
    _find_action(context['action_name'])(context)


if __name__ == '__main__':
    main()
