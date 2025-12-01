from PB.recipe_modules.depot_tools.gsutil import properties


DEPS = [
  'recipe_engine/context',
  'recipe_engine/file',
  'recipe_engine/path',
  'recipe_engine/platform',
  'recipe_engine/step',
]

ENV_PROPERTIES = properties.EnvProperties

from .api import GSUtilApi as API
