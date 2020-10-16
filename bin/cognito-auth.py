#!/usr/bin/env python3

"""
  generate SRP Authorization header string token value
  useful to help write automated tests or perform manual tests
  NOTE: "username" is usually an email address
  example:

    un="hi@example.com"
    curl -H "Authorization:$(cognito-auth.py ${un})" "https://test-api.example.com/login/${un}"
"""

import boto3, sys, warrant.aws_srp  # RCF-2945 Secure Remote Password

cognito_pools = { "default":"MY-test",  # Cognito pool names created in AWS console
    "MY-test":{"id":"us-east-1_wPcs69RIO","client_id":"hei57firt232p5oi4dhgclp0cd"},
    "MY-dev":{"id":"us-east-1_v357mag9m","client_id":"99bottle50fb33rh0fahd5rubz"}
    }

def generateIdToken(username,pool=cognito_pools["default"]):
    tokens = warrant.aws_srp.AWSSRP( username=username, password='sekretPassFromEnv',
                                    pool_id=cognito_pools[pool]["id"],
                                    client_id=cognito_pools[pool]["client_id"],
                                    client=boto3.client('cognito-idp', region_name='us-east-1')
                                ).authenticate_user()
    return tokens['AuthenticationResult']['IdToken']


if __name__ == '__main__':
    username = sys.argv[1] if len(sys.argv)>1 else "my.test@example.com"
    username += '' if '@' in username else "@example.com"
    cognito_pool = sys.argv[2] if len(sys.argv)>2 else cognito_pools["default"]
    id_token = generateIdToken(username,cognito_pool)
    if "-q" not in sys.argv:
        print("--header Authorization:{id_token}  # for %s in AWS Cognito pool %s (%s bytes)" % (
            username, cognito_pool, len(id_token))
            ,file=sys.stderr)
    print(id_token)
