DEPS = [
    'git',
    'gitiles',
    'recipe_engine/buildbucket',
    'recipe_engine/context',
    'recipe_engine/file',
    'recipe_engine/json',
    'recipe_engine/path',
    'recipe_engine/platform',
    'recipe_engine/properties',
    'recipe_engine/raw_io',
    'recipe_engine/step',
    'tryserver',
]

from .config import config_ctx as CONFIG_CTX

from .api import GclientApi as API
from .test_api import GclientTestApi as TEST_API

__all__ = ['CONFIG_CTX', 'API', 'TEST_API']

