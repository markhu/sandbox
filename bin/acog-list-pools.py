#!/usr/bin/env python3

# acog-list-pools.py -- list-user-pools from AWS Cognito

import boto3

sesh = boto3.Session(region_name="us-east-1")  # profile_name='default')  # ~/.aws/credentials
cog_client = sesh.client('cognito-idp')

response = cog_client.list_user_pools(MaxResults=60)

print('list_user_pools: (ID, Name)')

for x in response['UserPools']:
    print(f'    "{x["Id"]}", "{x["Name"]}"')

