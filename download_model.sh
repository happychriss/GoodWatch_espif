#!/bin/bash
project_id='43692'
project_name='ff_command_set_final'
set -e
echo "Start Download..."
curl --request GET \
  --url "https://studio.edgeimpulse.com/v1/api/${project_id}/deployment/download?type=arduino" \
  --header 'Accept: application/zip' \
  --header 'x-api-key: '''${1}''\
  --output arduino_model.zip
unzip -q arduino_model.zip -d ./src/edgeimpulse_tmp
rm -r ./src/edgeimpulse/
mkdir ./src/edgeimpulse
mv ./src/edgeimpulse_tmp/ff_command_set_final_inferencing/src/* ./src/edgeimpulse
rm -r ./src/edgeimpulse_tmp/
rm ./arduino_model.zip
find ./src/edgeimpulse -type f -exec touch {} +
echo "Downloaded model in folder src/edgeimpulse"
