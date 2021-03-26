#!/usr/bin/env python3

# acog.py = "Admin AWS Cognito"
# helper script adds/lists Cognito pool logins, sets custom attributes
# run from Jenkins or local command-line
# requirements: AWS credentials in environment vars or ~/.aws
#    some operations require: VPN network access for metadata API host

import boto3, botocore
import argparse
import csv
import datetime
import json
import logging, os
import requests
import sys
import time


defAction = "list-pools"
defBook = "5399020"
defDate = "try-API"  # flag to look up actual depart/embark date(s) in Metadata booking
defNames = "approve,always"
def_pool_name = "(1stAvailable)"
emailVal = defEmail = "%s@test.com"  # defaults to being a template/suffix after bookingNo
futureDate = (datetime.datetime.now() + datetime.timedelta(days=90)).strftime("%Y-%m-%d")
nonConsumerUserTypes = ("CSA","AIR","TA")  # TA=TravelAgent
paxArray = [{"BookingStatus":0,"Currency":"YEN","TourName":0,"TourStatus":0,"EmbarkDate":"2020-10-22",
             "GuestDepartureDate":"2020-10-22","Office":"JP"},
            {"pax_1":None},{"pax_2":None}]
pax1note = pax2note = "(no pax yet)"
user_count = 0

AWS_DEFAULT_REGION = "us-east-1"  # override env var or profile_name='default')  # ~/.aws/credentials
FATAL_ERROR_CODE = "FATAL_ERROR_CODE"
NON_FATAL_WARNING = "NON_FATAL_WARNING"
USER_GUID_ERROR = "USER_GUID_ERROR"
MATCH_FROM_POOL = "matchFromInternalList"
clientID = MATCH_FROM_POOL
clientIDs = {  # NOTE: from ~/git/COG-client/stack/envs/*
  "us-east-1_W0vAsS6HQ":{"id":"2131qREDACTED174crs","pool":"COG-dev"},
  "us-east-1_k9PQUnVgU":{"id":"6p4neREDACTEDqej1nf","pool":"COG-qa"},
  "us-east-1_BQlmg4kuQ":{"id":"5u11dREDACTED6aq4iu","pool":"COG-stage"}
   }  # id=ClientID passed to Lambda triggers for some Cognito operations --not an attribute of the entries.
local_client_ids_by_pool_name = {clientIDs[Id]["pool"]:Id for Id in clientIDs}
localPools = [{"Id":Id,"Name":clientIDs[Id]["pool"]} for Id in clientIDs]  # shape/style of aws cognito-idp list-user-pools

defPass = "noSmok!ng0316"
# depending on network routing, may not work from the cloud like it does from the office/VPN
Metadata = {"dev":{ "ip":"10.191.9.71","host":"apiqaa01.dev.nosmokingrc.com"},
             "qa":{ "ip":"10.191.9.71","host":"apiqaa01.dev.nosmokingrc.com"},
            "stg":{ "ip":"10.191.9.73","host":"apistg01.nosmokingrc.com"} }

parser = argparse.ArgumentParser(
             description=" AWS Cognito helper script, creates COG logins from Metadata booking")
parser.add_argument("action", default=defAction, nargs='?',
                    help="list-pools (default), list-users (by email), add-user (by bookingId), get-user (by GUID)")
parser.add_argument("arg2", default=defBook, nargs='?',
                    help="booking = {defBook} (can also specify full email address)".format(defBook=defBook))
parser.add_argument("arg3", default=def_pool_name, nargs='?',
                    help="poolID = {def_pool_name} (can specify user-friendly name, or ID)".format(def_pool_name=def_pool_name))
parser.add_argument("--clientID", default=clientID,
                    help="clientID = {clientID} (from AWS GUI console)".format(clientID=clientID))
parser.add_argument("--email", default=emailVal, help="defaults to <bookingId>@test.com")
parser.add_argument("--names", default=defNames, help="lastname,firstname")
parser.add_argument("--date", default=defDate, help="embark date format example: 2020-11-13")
parser.add_argument("--file", help="CSV/TSV data file: booking,lastname,firstname")
parser.add_argument("--profile", default="default", help="default AWS env or ~/.aws/credentials")
parser.add_argument("--region", default=AWS_DEFAULT_REGION, help="default: " + AWS_DEFAULT_REGION)
parser.add_argument("--attrib_name", default="custom:booking", help="for attrib-add action")
parser.add_argument("--attrib_val", default="[]", help="for magic add-attribs action")  # .attribs
parser.add_argument("--userType", default="Consumer", help="alt: CSA,Air,TA; default: Consumer")
parser.add_argument("--verbose",'-v', action="count", default=0, help="logging.INFO level (-vv for DEBUG)")
parser.add_argument("--allowUpperCaseEmail", action="store_true", default=False,
                    help="skip lowercase/normalize email")
parser.add_argument("--forceOldPass", default="auto",
                    help="default=auto (y for Consumer/test, n for agent/prompt)")
parser.add_argument("--skipMetadata", action="store_true",default=False)  # allow bypass of Metadata API lookups
args = parser.parse_args()

logging.basicConfig(level=[logging.WARNING,logging.INFO,logging.DEBUG][args.verbose])
logger = logging.getLogger(os.path.basename(__file__)+" ")

emailVal = args.email  # initialized global, for later override/parse/templatize/validation

if args.names != defNames:
    if ',' in args.names:
        firstName = fName = args.names.split(',')[-1]  # comma
        lastName = lName = args.names.split(',')[0]    # lastname,firstname
    else:     # initialize names --parse arg as delimited pair
        firstName = fName = args.names.split('.')[0]  # dot
        lastName = lName = args.names.split('.')[-1]  # firstname.lastname
elif '.' in args.email.split('@')[0]:
    firstName = fName = args.email.split('@')[0].split('.')[0]  # names from email
    lastName = lName = args.email.split('@')[0].split('.')[1]  # firstname.lastname@dom.com
else:
    firstName = fName = args.names.split(',')[-1]  # comma
    lastName = lName = args.names.split(',')[0]    # lastname,firstname

# first API call: to AWS
try:  # initial AWS/boto3 call to see if AWS can auth/connect using current profile
    if args.profile == "default":
        sesh = boto3.Session(region_name=args.region)  # auto-sense AWS credentials from env/files
        logger.info("default profile --region %s" % args.region)  # DEBUG/VERBOSE
    else:
        sesh = boto3.Session(region_name=args.region,profile_name=args.profile)
        logger.info(" --profile %s --region %s" % (args.profile,args.region))  # DEBUG/VERBOSE
except Exception as e:
    sesh = boto3.Session(region_name=args.region)  # env AWS_ACCESS_KEY_ID, AWS_SECRET_ACCESS_KEY
    logger.error(" %s by %s" % (e,__file__))  # e.g. config profile (blah) could not be found
    sys.exit(1)  # fatal error
logger.debug("dir(sesh): %s" % dir(sesh))

profile_name = sesh.profile_name if sesh.profile_name else "(%s)" % args.profile
region_name = sesh.region_name if sesh.region_name else "(%s)" % args.region
logger.debug(" using AWS/boto3 session --profile %s --region %s" % (
                    profile_name, region_name))

if sesh.get_credentials().method == "sso":  # and sesh.get_credentials().refresh_needed():
    # logger.error("%s" % sesh.get_credentials())
    try:
        sso_cred = sesh.get_credentials().get_frozen_credentials()
    except Exception as e:
        logger.error("%s\n--Try 'aws sso login' to cure." % e)
        sys.exit(1)

if 'cognito-idp' in sesh.get_available_services():  # ~~~~~~~~ GLOBAL ~~~~~~~~
    logger.debug("TRACE: pre-COG-client")  # DEBUG/VERBOSE --TRACE only shows with 2 or more -vv
    cog_client = sesh.client('cognito-idp')  # cog_client = boto3.client('cognito-idp')
else:
    logger.error("AWS service 'cognito-idp' not available for profile %s" % sesh.profile_name)
    sys.exit(1)

try:
  # cache list-pools/list-user-pools
  logger.debug("TRACE: pre-COG-list_user_pools")  # DEBUG/VERBOSE
  userPools = cog_client.list_user_pools(MaxResults=60)  # we only had ~13 as of August 2019
  logger.debug("TRACE: post-COG")  # DEBUG
except (botocore.exceptions.UnauthorizedSSOTokenError,
        botocore.exceptions.SSOError,
        botocore.exceptions.SSOTokenLoadError
       ) as e:
    logger.info("Try 'aws sso login' for %s exception:\n  %s" % (type(e),(e)))  # DEBUG/VERBOSE
    sys.exit(1)  # fatal error
except botocore.exceptions.ClientError as e:
    print("ERROR: AWS botocore client exception:\n  %s" % (e))
    logger.info("ERROR: AWS botocore exception.response:\n%s" % (e.response))
    if e.response["Error"]["Code"] in ("UserLambdaValidationException",
        "UnauthorizedSSOTokenError", "AccessDeniedException"):
        print("WARN: AWS botocore client exception %s" % (e))
        sys.exit(1)  # fatal error
except Exception as e:
    logger.error(" AWS  cognito-idp list-user-pools --profile %s"  % profile_name)
    logger.error("  --region %s\n" % (region_name, e))
    sys.exit(1)  # fatal error

# the un-named args handling is not-great...
if args.arg2.find('-') >= 0:  # dash implies action or pool
    bookingId = args.arg3.split('@')[0] if "COG-" not in args.arg3 else None
    emailVal = args.arg3 if '@' in args.arg3 and '%s' in args.email else args.email  # override
    longAction = args.action if "COG-" not in args.action else defAction  # default to read-only
    UserPoolName = args.arg2 if "COG-" in args.arg2 else def_pool_name
    logger.debug("UserPoolName: %s" % (UserPoolName))
else:  # ToDo: more parse validation of input parameters...
    bookingId = args.arg2.split('@')[0] if "COG-" not in args.arg2 else None
    emailVal = args.arg2 if '@' in args.arg2 and '%s' in args.email else args.email  # override
    UserPoolName = args.arg3 if "COG-" in args.arg3 else def_pool_name

UserPoolName = "COG-" if "list-p" in args.action else UserPoolName
logger.debug("UserPoolName: %s" % (UserPoolName))

argX = bookingId if bookingId else emailVal  # stash for later list-user filter in doListUsers
bookingId = bookingId if str(bookingId).isnumeric() else None  # no bookingId implies AIR/CSA later
if bookingId is None and emailVal[0:7].isnumeric():
    bookingId = emailVal[0:7]

if '%s' in emailVal and args.file is not None:  # templatize conditionally
    if bookingId:
        emailVal = emailVal % (bookingId)
    elif ',' in args.names and args.names not in ("lastName,firstName",defNames):
        emailVal = emailVal % (firstName + '.' + lastName)
    elif args.names in ("lastName,firstName",defNames):
        print("    emailVal %s (to be determined later from CSV)" % emailVal)
    else:
        emailVal = emailVal % (args.names)

# determine pool ID from pool name
local_client_ids_by_pool_name = {clientIDs[Id]["pool"]:Id for Id in clientIDs}
avail_client_ids_by_pool_name = {userPool["Name"]:userPool["Id"] for userPool in userPools['UserPools']}
avail_client_pool_names = sorted([*avail_client_ids_by_pool_name.keys()])

oldUserPoolName = UserPoolName
UserPoolName = avail_client_pool_names[0] if UserPoolName == def_pool_name else UserPoolName
if UserPoolName in avail_client_ids_by_pool_name:  # exact match has priority
    UserPoolId = avail_client_ids_by_pool_name[UserPoolName]
else:
    for avail_pool_name in avail_client_ids_by_pool_name:
        if UserPoolName in avail_pool_name:  # partial match as fallback
            UserPoolId = avail_client_ids_by_pool_name[avail_pool_name]
            UserPoolName = avail_pool_name
            break  # exit loop on finding first match
        else:
            UserPoolId = None

logger.info(" UserPoolId: %s from %s" % (UserPoolId, avail_client_ids_by_pool_name))
print("UserPoolName:     '%s' from %s ~ %s" % (
    UserPoolName,oldUserPoolName,avail_client_pool_names),file=sys.stderr)

if UserPoolId is None or "COG-" in UserPoolId:  # poolID not yet mapped from poolName
    logger.warning("UserPoolId %s not found for UserPoolName %s !?!" % (UserPoolId,UserPoolName))
    print("  (...at least not with --profile '%s')" % (profile_name),file=sys.stderr)
    sys.exit(1)  # fatal exit

logger.debug("DEBUG: clientID: %s" % (clientID))
if clientID == MATCH_FROM_POOL and "list-p" not in args.action:
    clientID = clientIDs[UserPoolId]["id"]
else:
    clientID = args.clientID
logger.debug("DEBUG: clientID: %s" % (clientID))

# map pool names to Metadata host per environment
if "stage" in UserPoolName or "abe" in UserPoolName or "st" in UserPoolName:
    MetadatahostIp = Metadata["stg"]["ip"]
    MetadatahostName = Metadata["stg"]["host"]
    if "abe" in UserPoolName:
        clientID = clientIDs[UserPoolId].get("abe_client_id",clientID) if clientID == MATCH_FROM_POOL else clientID
    else:  # stage
        clientID = clientIDs[UserPoolId]["id"]
else:
    MetadatahostIp = Metadata["dev"]["ip"]  # ["host"]  # QA-4779  # Dev & QA share Metadatahost
    MetadatahostName = Metadata["dev"]["host"]
logger.info(" clientID: %s" % (clientID))

if args.verbose:
    print("         emailVal:%s (per args.email:%s)" % (emailVal,args.email),file=sys.stderr)

try:
    userPoolConfiguration = cog_client.describe_user_pool(UserPoolId=UserPoolId)
except botocore.exceptions.ClientError as e:  # ResourceNotFoundException as e:
    print("%s (...at least not with --profile '%s')" % (e.response["Error"]["Message"],
          profile_name),file=sys.stderr)
    sys.exit()  # fatal error

# below removes redundant configuration that do not work when passed together
try:
    if (userPoolConfiguration['UserPool']['Policies']['PasswordPolicy']['TemporaryPasswordValidityDays']):
        userPoolConfiguration['UserPool']['AdminCreateUserConfig'].pop('UnusedAccountValidityDays')
except KeyError:
    pass

userPoolConfigAttribs_base = [ 'AdminCreateUserConfig', 'AutoVerifiedAttributes', 'DeviceConfiguration',
      'EmailConfiguration', 'EmailVerificationMessage', 'EmailVerificationSubject',
      'MfaConfiguration', 'Policies',
      'SmsAuthenticationMessage', 'SmsConfiguration', 'SmsVerificationMessage',
      'UserPoolAddOns', 'UserPoolTags', 'VerificationMessageTemplate'
      ]

# ~~~~~~~~~~~~~~~~~~~~~~~~~~~ function definitions ~~~~~~~~~~~~~~~~~~~~~~~~~~~

def doDisableTriggers():  # warning: stateful/race-condition
  attribs = userPoolConfigAttribs_base
  conf  = {x:userPoolConfiguration['UserPool'][x] for x in attribs if x in userPoolConfiguration['UserPool']}
  try:
      cog_client.update_user_pool(UserPoolId=UserPoolId, LambdaConfig={}, **conf)
  except Exception as e:
      print("WARN: AWS/boto3 exception in triggers disable: %s" % (e))


def doEnableTriggers():  # ToDo: find a way to _ALWAYS_ restore --add to any/all try/except exception handling...
  attribs = userPoolConfigAttribs_base + ['LambdaConfig']  # restore Lambda Cognito trigger
  conf  = {x:userPoolConfiguration['UserPool'][x] for x in attribs if x in userPoolConfiguration['UserPool']}
  try:
      cog_client.update_user_pool(UserPoolId=UserPoolId, **conf)
  except Exception as e:
      print("WARN: AWS/boto3 exception in triggers [re]enable : %s" % (e))


def printHeadings(longAction, bookingId, UserPool,userGUID=None,embarkDate=futureDate,embarkNote="default"):
    print("action:            " + longAction)
    print("bookingId/email:   %s" % bookingId, end='')
    print(" / " + emailVal)
    if "MER" in args.userType.upper(): # match 'CONSUMER'
        print("embarkDate:        " + str(embarkDate) + " (" + str(embarkNote) + ")")  # indicate if from-API or args
    print("UserPool:          " + UserPool + " (" + UserPoolName + ")")
    if userGUID:
      print('userGUID:         "{userGUID}"'.format(userGUID=userGUID))
    return


def doAdminCreateUser(un,upi=UserPoolId,firstName=firstName,lastName=lastName,dates={},paxnum=1,emailVal=None,tourName=None):
    # func called by doAddUser
    ''' aws cognito-idp admin-create-user --color=on --user-pool-id=us-east-1_o1BRMLLH8 --username 5399020@test.com \
          --temporary-password ${defPass} '--user-attributes={"(JSON..)"}' \
          | jq .User.Username -r

    return GUID
    '''

    # should not overwrite global-as-template, in order to handle batch CSV file looping
    # global emailVal  # template should have been done during args processing
    if args.allowUpperCaseEmail==False:
        un = un.lower()
    emailVal = un if '@' in un else (emailVal if emailVal else defEmail)
    if emailVal in (defEmail,"firstName.lastName@test.com"):
        emailVal = "%s@%s" %(un,emailVal.split('@')[-1])

    departureDate = dates.get('departureDate',futureDate).split('.')[0]  # strip off fractional second, if any
    embarkDate =    dates.get('embarkDate',futureDate).split('.')[0]  # strip off fractional second, if any

    if args.userType.upper() in nonConsumerUserTypes:  # TODO: pass in specific value (to handle CSV) --not global args
        if "%s" in emailVal or emailVal[0:7].isnumeric():
            emailVal = "%s.%s@%s" %(firstName,lastName,emailVal.split('@')[-1])
        print("    DEBUG: (un:%s, %s , emailVal:%s, (userType %s no bookingId)\n" % (un,'_' * 19,emailVal,args.userType) if args.verbose else '',end='')
        attribs = [ {"Name":"custom:userType", "Value": args.userType  # case as-is
          },{"Name":"email", "Value":emailVal
          },{"Name":"email_verified", "Value":"true"
          },{"Name":"family_name","Value":lastName
          },{"Name":"given_name","Value":firstName
          } ]
    elif "MER" in args.userType.upper(): # match 'CONSUMER'
        print("    DEBUG: (un:%s, %s , emailVal:%s, bookingId:%s)\n" % (un,'_' * 19,emailVal,bookingId) if args.verbose else '',end='')
        attribs = [ { "Name":"custom:userType", "Value": args.userType
          },{"Name":"custom:booking", "Value":'[%s]' % json.dumps(
                   {"bookingId":bookingId,"tourName":tourName,"departureDate":departureDate,
                    "embarkDate":embarkDate,"passengerNumber":paxnum})
          },{"Name":"email", "Value":emailVal
          },{"Name":"family_name","Value":lastName
          },{"Name":"given_name","Value":firstName
          } ]
    else:
        attribs = [ { "Value": ["ERROR UNSUPPORTED custom:userType", args.userType]} ]

    if args.verbose:
        print("DEBUG: Username=emailVal='%s' attribs: '%s'" % (emailVal,attribs))

    try:
      r = cog_client.admin_create_user(
                 UserPoolId=upi,
                 Username=emailVal,
                 TemporaryPassword=defPass,
                 UserAttributes=list(attribs),
                 MessageAction="SUPPRESS"  # don't send SMS or email
      )
    except botocore.exceptions.ClientError as e:
      if e.response["Error"]["Code"] in ("UserLambdaValidationException",
                                         "AccessDeniedException"):
          logger.error("Exception.response: %s" % (json.dumps(e.response["Error"],indent=4,sort_keys=True)))
          return("%s:%s" % (FATAL_ERROR_CODE, e.response["Error"]["Code"]))
      if e.response["Error"]["Code"] in ["InvalidParameterException",
                                         "UnexpectedLambdaException"
                                        ]:
          print("Exception.response: %s" % (json.dumps(e.response,indent=4)))
          print("BLOCKED ON AWS UI-CONTROLLED SETTING: Need to toggle Pre sign-up trigger! --URL:")
          print("  https://console.aws.amazon.com/cognito/users/?region={region}#/pool/{upi}/triggers".format(
                           region=args.region, upi=upi))
      elif e.response["Error"]["Code"] == "UsernameExistsException":
          print('    CAUGHT: %s "%s" (%s)' % (e.response["Error"]["Code"],emailVal,
                                              e.response["Error"]["Message"]))
          retry = cog_client.list_users(UserPoolId=UserPoolId,Filter='email ^= "%s"' % (emailVal))
          print('     "UserLastModifiedDate":"%s"' % retry["Users"][0]["UserLastModifiedDate"] if retry["Users"] else "wtf")
          # return("%s:%s" % (NON_FATAL_WARNING, e.response["Error"]["Code"]))
          response = cog_client.list_users(UserPoolId=UserPoolId,Filter='email ^= "%s"' % (un))

          print("    PRIOR USER:", end=' ')
          return(retry["Users"][0]["Username"] if retry["Users"] else None)  # pre-existing GUID from list-users --assume only 1 hit
      else:
          print("Exception.response: %s" % (json.dumps(e.response,indent=4)))
          return(USER_GUID_ERROR)
    else:  # non-exception
      r['User'].pop('UserCreateDate',None)  # remove element(s) with non-JSON-ifiable value(s)
      r['User'].pop('UserLastModifiedDate',None)
      # print("Success response (JSON) : %s" % (json.dumps(r,indent=4)))
      # print("Success response (interesting bits) : %s" % (json.dumps(
      #   {"User":{"Username":r["User"]["Username"],"UserStatus":r["User"]["UserStatus"]
      #   }},indent=4)))
      return(r['User']['Username'])  # new GUID from admin-create-user response


def doAddAttribs(UserPoolId=UserPoolId,Username='userGUID aka "sub"',UserAttributes=[]):
    logger.info("\n   UserPoolId:{UserPoolId}\n     Username:{Username}\nUserAttributes ...\n    {UserAttributes}".format(
        UserPoolId=UserPoolId,
        Username=Username,
        UserAttributes=UserAttributes)
    )
    r = cog_client.admin_update_user_attributes(
        UserPoolId=UserPoolId,
        Username=Username,
        UserAttributes=UserAttributes)
    return(r)


def doAddUser(un,upi=UserPoolId,fName=firstName,lName=lastName,dates={},paxArray=paxArray):
    # this higher-level function calls steps required to create an entry in Cognito, bump confirmation
    global user_count

    dates['departureDate'] = str(dates.get('departureDate',futureDate)).split('T')[0]  # strip off hours:minutes, if any
    dates['embarkDate'] =    str(dates.get('embarkDate',futureDate)).split('T')[0]  # strip off hours:minutes, if any
    print("  step 1. admin-create-user %s in %s" %(un,upi), end='\n')
    userGUID = doAdminCreateUser(un=un,upi=UserPoolId,firstName=fName,lastName=lName,
        dates=dates,emailVal=emailVal,tourName=paxArray[0]["TourName"])
    print("      --userGUID: %s" % userGUID)  # ToDo: display aws cli-equivalent command...
    if userGUID in (None, USER_GUID_ERROR) or FATAL_ERROR_CODE in userGUID:
        return userGUID

    print("  step 2. admin-update-user-attributes (via doAddAttribs())")
    # optional for new-users, but useful to break this out for updating existing
    # aws cognito-idp admin-update-user-attributes \ 
    #   --user-pool-id=${poolID} --username ${userGUID} --user-attributes="Name=email_verified,Value=true"
    UserAttributes=[{"Name":"email_verified","Value":"true"}
                   ,{"Name":"custom:userType", "Value":args.userType}]  # AIR QA-4780

    try:
      r = doAddAttribs(UserPoolId=UserPoolId,Username=userGUID,UserAttributes=UserAttributes)
    except cog_client.exceptions.LimitExceedException as e:
        logger.warn("API call limit exceeded; backing off and retrying...\n  %s" % e)
        time.sleep(0.5)
        r = doAddAttribs(UserPoolId=UserPoolId,Username=userGUID,UserAttributes=UserAttributes)
    except Exception as e:
        logger.warn("WARN: AWS/boto3 exception: %s" % (e))

    print("  step 3. admin-initiate-auth")
    # aws cognito-idp admin-initiate-auth --user-pool-id=${poolID} --client-id="${clientID}" \ 
    # --auth-flow ADMIN_NO_SRP_AUTH --auth-parameters "USERNAME=${userGUID},PASSWORD=${defPass}"
    if args.verbose:
      print("""    $ aws cognito-idp admin-initiate-auth --user-pool-id={UserPoolId} --client-id='{clientID}' \\
        --auth-flow ADMIN_NO_SRP_AUTH --auth-parameters 'USERNAME={userGUID},PASSWORD=...'""".format(
         UserPoolId=UserPoolId,clientID=clientID,userGUID=userGUID))
    try:
      r = cog_client.admin_initiate_auth( UserPoolId=UserPoolId, ClientId=clientID,
              AuthFlow="ADMIN_NO_SRP_AUTH", AuthParameters={"USERNAME":userGUID,"PASSWORD":defPass}
      )
    except botocore.exceptions.ClientError as e:
      if e.response["Error"]["Code"] in ("NotAuthorizedException"):
        print("    CAUGHT: %s (%s)" % (e.response["Error"]["Code"],e.response["Error"]["Message"]))
        print("      --trying adminSetUserPassword...")
        r = cog_client.admin_set_user_password(UserPoolId=UserPoolId,Username=userGUID,
                                                Password=defPass,Permanent=True)

      elif e.response["Error"]["Code"] in ("UserLambdaValidationException"):
        print("    CAUGHT WARNING: AWS/boto3 exception: %s" % (":\n      ".join(str(e).split(": "))))
        if (args.forceOldPass in ("auto","n","N",'0')) and args.userType.upper() in nonConsumerUserTypes:  # CSA,AIR,TAP
            print("    ADD userType:%s (skipping respond-to-auth ChallengeName %s)" % (args.userType,r.get("ChallengeName")),end='')
            print("  --forceOldPass=y can update this (idempotent)" if (args.verbose) else '')
        else:
            print("      --trying adminSetUserPassword...")  # workaround for COR-316
            r = cog_client.admin_set_user_password(UserPoolId=UserPoolId,Username=userGUID,
                                               Password=defPass,Permanent=True)
        # user_count += 1; return("%s:%s" % (NON_FATAL_WARNING, e.response["Error"]["Code"]))
      else:
        print("    WARN: sleep 0.5s then retrying once after AWS/boto3 exception: %s" % (e))
        time.sleep(0.5)  # ToDo: intelligently retry?!? and/or parse result of previous API call?
        r = cog_client.admin_initiate_auth( UserPoolId=UserPoolId, ClientId=clientID,
              AuthFlow="ADMIN_NO_SRP_AUTH", AuthParameters={"USERNAME":userGUID,"PASSWORD":defPass}
        )

    print("  step 4. admin-respond-to-auth-challenge")
    if r.get("ChallengeName") == None:
        print("    (no challenge pending)")
    elif (args.forceOldPass in ("auto","n","N",'0')) and args.userType.upper() in nonConsumerUserTypes:  # CSA,AIR,TAP
        print("    ADD userType:%s (skipping respond-to-auth ChallengeName %s)" % (args.userType,r.get("ChallengeName")),end='')
        print("  --forceOldPass=y can update this (idempotent)" if (args.verbose and r.get("ChallengeName")) else '')
    elif r and args.forceOldPass in ("auto","y","Y",'1'):
        if r.get("Session") is not None and r.get("ChallengeName") in ("FORCE_CHANGE_PASSWORD","NEW_PASSWORD_REQUIRED"):
            seshVals=r["Session"] ; seshVals_REDACTED='REDACTED(long,boring & ephemeral)'
            print('    $ aws cognito-idp admin-respond-to-auth-challenge --user-pool-id {} --client-id "{}"'.format(
                               UserPoolId,clientID), end=' \\\n')
            print('      --session {seshVals} --challenge-name {ChallengeName} '.format(
                               seshVals=seshVals_REDACTED,ChallengeName=r["ChallengeName"]),end=' \\\n')
            print('      --challenge-responses "NEW_PASSWORD=...,USERNAME={userGUID}"'.format(userGUID=userGUID))
            r = cog_client.admin_respond_to_auth_challenge( UserPoolId=UserPoolId, ClientId=clientID, Session=seshVals,
                      ChallengeName=r["ChallengeName"],ChallengeResponses={"USERNAME":userGUID,"NEW_PASSWORD":defPass}
            )
            print("    ADDED USER:", end=' ')
        else:
            print("    PRIOR USER:", end=' ')
    else:
        print("    MUNGED USER:", end=' ')    # rare

    doListUsers(Filter=('email ^= "%s"' % (un)),UserPoolId=UserPoolId)

    return userGUID


def doListUsers(Filter,UserPoolId=UserPoolId,bookingId=bookingId,verbosityLevel=0):
    # list-users called individually after create, or with filter to list matches
    global user_count
    Filter = 'email ^= "%s"' % Filter if '"' not in Filter else Filter
    bookingId = ''.join(filter(str.isdigit, Filter)) if len(str(bookingId)) != 7 else bookingId
    print("  $ aws cognito-idp list-users --filter '{Filter}'".format(Filter=Filter), end=' ')
    print("--region '%s' --user-pool-id '%s' | jq '.Users[]'" % (args.region,UserPoolId))
    if args.verbose:
        print("        DEBUG: bookingId: %s, emailVal: %s, Filter: %s" % (bookingId, emailVal, Filter))

    cog_response = cog_client.list_users(UserPoolId=UserPoolId,Filter=Filter,Limit=60)

    if len(cog_response["Users"]) == 0 or args.verbose:
        print("(%d users matched Cognito filter '%s')" % (len(cog_response["Users"]), Filter))

    for user in cog_response["Users"]:
        user_count += 1
        attr_count = 1
        attribs = user.pop("Attributes")  # attribs list esp. UserStatus in(CONFIRMED,FORCE_CHANGE_PASSWORD)
        attribs.append(user)   # flatten structure for consistent output format including metadata
        for attr in sorted(attribs,key=lambda d:d.get("Name",'a'),reverse=False):
            attrAsStr = str(attr) if '"' in attr.get("Value","single-quoter") else json.dumps(
                            attr,sort_keys=True,default=str)  # specify default=function to dump datetime object)
            if attr_count == 1:
                print('  %s.   %s' % (user_count,attrAsStr))  # outdent first line
            else:
                print('   .%s  %s' % (attr_count,attrAsStr))  # indent remaining lines
            attr_count += 1

    if len(str(bookingId)) == 7 and args.file is None:
        callMetadata(bookingId,verbosityLevel=verbosityLevel)

    return user_count


def callMetadata(bookingId,verbosityLevel=0):  # returns a large-ish dict structure
    global pax1note
    try:
        url = "http://%s:8080/api/booking/getdetails/%s" % ( MetadatahostIp, bookingId)
        logger.info(("Metadata url %s (%s)\n" % (url,MetadatahostName)) if (verbosityLevel > 0) else '')
        r = requests.get(url, timeout=9)
        assert r.json()[0]["BookingNo"], "Invalid booking # %s" % bookingId  # raise 
    except Exception as e:
        embarkDate=futureDate
        departDate=embarkDate
        embarkNote="default"
        pax1note = pax2note = "ERROR"
        print("Exception %s" % e)  # https://martinfowler.com/articles/microservices.html#SmartEndpointsAndDumbPipes
        print("Could not get live embarkDate from Metadata %s --using arg/defaults..." % url)
        return {"embarkNote":embarkNote,"pax1note":pax1note,"pax2note":pax2note,"paxArray":paxArray}
    else:
        if r.json()[0]["BookingNo"]:
            for k in paxArray[0].keys():
                paxArray[0][k] = r.json()[0].get(k)  # booking info that applies to all pax
            departDate = r.json()[0]["GuestDepartureDate"].split('T')[0]
            embarkDate = r.json()[0]["EmbarkDate"].split('T')[0]
            embarkNote = "API elapsed %s (departDate:%s) " % (r.elapsed,departDate)
            paxArray[0]["EmbarkDate"] = embarkDate
            paxArray[0]["GuestDepartureDate"] = departDate

            # print("\nDEBUG: paxArray: %s" % json.dumps(paxArray,sort_keys=True))
            paxArray[0]["paxCount"] = len(r.json()[0]["Passengers"])
            for paxObj in r.json()[0]["Passengers"]:
                if args.verbose and verbosityLevel > 1:
                    print("\nDEBUG: paxObj (paxArray[%s]): %s" % (paxObj['paxnum'],json.dumps(paxObj,sort_keys=True)))
                paxArray[paxObj['paxnum']] = paxObj
            fName = paxArray[1]["FirstName"]
            lName = paxArray[1]["LastName"]  # override args.names with API results
        else:
            embarkDate=futureDate
            embarkNote="default --API error Details: %s" % (r.json()[0]["Details"])
            if args.verbose or verbosityLevel > 0:
                print("\nDEBUG: paxArray: %s" % json.dumps(paxArray,sort_keys=True))
        # Cognito can be picky about matching names to Metadata exactly
        # nice ToDo: list-comprehension dict-values-only
        pax1note = [paxArray[1]["paxnum"],paxArray[1]["Title"],paxArray[1]["FirstName"],
                    paxArray[1]["MiddleName"], paxArray[1]["LastName"],paxArray[1]["Suffix"]
                    ]
        pax2note = [paxArray[2]["paxnum"],paxArray[2]["Title"],paxArray[2]["FirstName"],
                    paxArray[2]["MiddleName"], paxArray[2]["LastName"],paxArray[1]["Suffix"]
                ] if paxArray[2].get("paxnum") else "(no pax2)"

        if args.verbose and verbosityLevel > 0:
            paxArray[0].pop("Pricing",None)
            paxArray[0].pop("ExtensionDetails",None)  # toss metadata we don't care about
            print("    %s" % (json.dumps(paxArray[0])))
            print(" \t %s" % (pax1note))
            print(" \t %s" % (pax2note))

    return {"embarkNote":embarkNote,"pax1note":pax1note,"pax2note":pax2note,"paxArray":paxArray}


def generateIdToken(username,pool=def_pool_name):
    import warrant.aws_srp  # RCF-2945 Secure Remote Password
    tokens = warrant.aws_srp.AWSSRP( username=username, password=defPass,
                                    pool_id=UserPoolId,
                                    client_id=clientID,
                                    client=boto3.client('cognito-idp', region_name=AWS_DEFAULT_REGION)
                                ).authenticate_user()
    return tokens['AuthenticationResult']['IdToken']


def Metadata_needed_flag(args,defNames):
    return (  # bool
                  ('API' in args.date.upper() or args.names == defNames)
              and (not args.skipMetadata and 'skip' not in args.date)
              and "CONSUMER" in args.userType.upper() )


if __name__ == "__main__":  # ~~~~~~~~~~~~~~~~~~~~~~~~~~~~ "main" ~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    if 'auth' in args.action:
        idToken = generateIdToken(emailVal,UserPoolId)  # handy for curl
        if args.verbose:
          print("DEBUG: UserPoolId:%s, clientID:%s" % (UserPoolId,clientIDs[UserPoolId]), file=sys.stderr)
          print("--header Authorization:{idToken}  # for %s in AWS Cognito pool %s (%s bytes)" % (
            emailVal, clientIDs[UserPoolId]["pool"], len(idToken))
            ,file=sys.stderr)
        print(idToken)  # handy for curl
        sys.exit(0)

    if args.action.startswith('l') or args.arg2.startswith('l'):  # l=list
      if 'u' in args.action and 'p' not in args.action or 'u' in args.arg2 and 'p' not in args.arg2:
        longAction = "list-users"
        Filter = 'email ^= "%s"' % (argX if bookingId in ('','None',None) else bookingId)
        if args.verbose:
            print("  DEBUG: argX: %s, bookingId: %s, emailVal: %s, Filter: %s" % (argX, bookingId, emailVal, Filter))
        doListUsers(Filter=Filter,UserPoolId=UserPoolId,bookingId=bookingId,verbosityLevel=1)
      elif 'p' in args.action or not 'u' in args.action:
        longAction = "list-pools"
        print("  aws cognito-idp list-user-pools --max-results=60",end=' ')
        print("--region '{region}'".format(region=args.region), end=' ')
        print("| jq '.UserPools[]|{Id,Name}' -c")
        for up in userPools['UserPools']:  # print cached pools list
            print('      "%s",       "%s"' % (up["Id"], up["Name"]))
    elif args.action.find('g') >= 0:  # get-user
        longAction = "get-user"
        print("ToDo: aws cognito-idp admin-get-user --username '%s' " % argX,end='')
        print("--region '%s' --user-pool-id '%s'" % (args.region,UserPoolId))
    elif args.action.find('add') >= 0:
        if args.action.find('att') >= 0:  # add-attribs or attrib-add
            longAction = "add-attribs"
        if args.action.find('u') >= 0:  # add-user or user-add
            longAction = "add-user"
    elif args.action.find('del') == 0:  # match like del-user or user-delete
        longAction = "delete-user"
        print("action: %s" % (longAction))
        emailVal = emailVal % (argX.split('@')[0]) if "%s" in emailVal else emailVal
        print("  $ aws cognito-idp admin-delete-user --user-pool-id %s --username %s  # bookingId:%s\n" % (UserPoolId,emailVal,bookingId))
        r = cog_client.admin_delete_user(Username=emailVal,UserPoolId=UserPoolId)
        if 200 == r['ResponseMetadata']['HTTPStatusCode']:
          print("SUCCESS: admin_delete_user %s  # bookingId:%s" % (emailVal,bookingId))
    else:
        print("                   ^ (unknown)")
        parser.print_help()

    # ~~~~~~~~ ~~~~~~~~ ~~~~~~~~ ~~~~~~~~
    if longAction == "add-user":
      if args.file:  # do 1+ user(s) loop through lines from text file (CSV,TSV)  # TODO: move this large block into its own function...
        print("Require at least 4 headers like this in CSV/TAB-delimited file:\n	INVOICE,LNAME,FNAME,DEPART (any order)",file=sys.stderr)
        with open(args.file, newline='', encoding='utf-8-sig') as tsvfile:  # f = open(args.file, mode='r')
          sample = tsvfile.read(1024)
          try:
            dialect = csv.Sniffer().sniff(sample,delimiters=', \t')  # auto-detect TSV,CSV
            delimiter = dialect.delimiter
            if ',' in sample:  # sometimes sniffer guesses wrong on cosmetic space-padding
              delimiter = ','
          except Exception as e:
            print("WARN: Exception csv.Sniffer: %s" % (e))
            if ',' in sample:
              print("  OK: deduced comma delimiter")
              delimiter = ','
            elif '\t' in sample:
              print("  OK: deduced TAB delimiter")
              delimiter = '\t'
            else:
              print("WARN: could not deduce delimiter, so forcing to TAB!")
              delimiter = '\t'

          tsvfile.seek(0)  # rewind after peek
          if args.verbose:
              print("DEBUG: dialect.delimiter: '%s'" % (delimiter))

          # pre-read to detect/fixup field headers
          reader = csv.DictReader(tsvfile, delimiter=delimiter)  # dialect can be CSV or TSV (or space?)
          fields = list()
          for fieldname in reader.fieldnames:
              field = fieldname.strip(" \ufeff")  # UTF-8 filter (maybe not needed after above encoding='utf-8-sig')
              if len(field) > 1:  # skip empty field/headings
                  fields.append(field.lower())  # normalize lowercase to ease matching below
          if args.verbose:
              print("DEBUG: fields: %s" % (fields))
          tsvfile.seek(0)  # rewind after peek

          # finally, get down to the real working looper... ##################
          reader = csv.DictReader(tsvfile, fieldnames=fields, delimiter=delimiter)  # dialect can be CSV or TSV (or space)
          next(reader)  # skip first line which is column headings line
          for line in reader:
            # Assign field values based on human-readable field names, depending
            # if they came from Sales tables, or another table, or abbreviated.
            # There's got to be a better way to match heading name variations (i.e. dict-data)
            # field names via Sales or Pax tables --all this "r0bust" handling is getting silly...
            bookingId = line.get("invoice", line.get("bookingid",
                        line.get("invoiceno", line.get("bookingno",    # QA-4778 case matters
                        line.get("invoicebooking", line.get("booking",
                                 '%s')))))).strip()  # magically handle userType AIR emails
            embarkDate = line.get("fromdate", line.get("fmdate", line.get("from",
                         line.get("embkdate", line.get("embarkdate",line.get("embark",
                                  '2020-05-04')))))).strip()  # NOTE: silly Star Wars day default
            departureDate = line.get("depart",line.get("departuredate",line.get("end",line.get("to",
                            line.get("todate",line.get("departdate",line.get("enddate",
                                     embarkDate))))))).strip()  # NOTE: defaults to embarkDate
            fName = line.get("fname", line.get("firstname",
                    line.get("fname1",line.get("fname2", 
                             firstName)))).strip()
            lName = line.get("lname",  line.get("lastname",
                    line.get("lname1", line.get("lname2",
                             lastName)))).strip()
            try:
              print("%s. file fields: %s" % (user_count + 1,
                [ bookingId, fName, lName, departureDate, embarkDate]),end='',file=sys.stderr)
            except:
              print("ERROR reading line %s:\n%s" % (user_count,line))

            if str(bookingId).isnumeric() and int(bookingId) > 999999 or args.userType in nonConsumerUserTypes:
              emailVal = defEmail % bookingId if "MER" in args.userType else emailVal
              print(" (ok)",file=sys.stderr)
              print(("\nDEBUG: emailVal: %s (pre call doAddUser)\n" % emailVal) if args.verbose else '',end='')

              r = doAddUser(un=bookingId, upi=UserPoolId, fName=fName, lName=lName,
                         dates={"departureDate":departureDate,"embarkDate":embarkDate})
            else:
              print(" WARN: (skipping comment/cosmetic/empty/header/malformed line)",file=sys.stderr)
              r = 'skip-a-line'

            if r == USER_GUID_ERROR:
              print("ERROR: USER_GUID_ERROR")
              # exit  # don't make this fatal--continue loop-processing
        print("DONE acog.py add-user %s  # count: %s" % (UserPoolId, user_count))

      else:  # do 1 user from command-line args ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
        # detect default/placeholder args and replace with actual booking/invoice data lookup
        if args.verbose:
            print("DEBUG: args.date       /       defDate : %s / %s" % (args.date,defDate))
            print("DEBUG: args.names      /      defNames : %s / %s" % (args.names,defNames))
            print("DEBUG: args.skipMetadata / args.userType : %s / %s" % (args.skipMetadata,args.userType))
        if Metadata_needed_flag(args,defNames):
            APIresults = callMetadata(bookingId,verbosityLevel=1)   # Metadata API delay slowdown (throttles Cognito pool writes.)
            embarkNote = APIresults["embarkNote"]
            pax1note = APIresults["pax1note"]
            pax2note = APIresults["pax2note"]

            paxArray = APIresults["paxArray"]
            departureDate = paxArray[0]["GuestDepartureDate"]
            embarkDate = paxArray[0]["EmbarkDate"]
            fName = paxArray[1]["FirstName"] if pax1note not in ("ERROR","(no pax yet)") else firstName
            lName = paxArray[1]["LastName"] if pax1note not in ("ERROR", "(no pax yet)") else lastName
        else:
            embarkDate = departureDate = args.date
            embarkDate = "(--date='%s' failed/skipped)" % embarkDate if str(embarkDate[0]).isalpha() else embarkDate
            embarkNote="args"
            # pax1note = pax2note = "args..."
            if args.verbose:
                print("DEBUG bookingId: %s" % (bookingId))
                print("DEBUG emailVal: %s" % (emailVal))

        userName = bookingId if bookingId else emailVal
        if args.verbose:
            print("DEBUG userName: %s" % (userName))
        embarkNote = embarkNote + '\n      %s\n      %s\n    & %s' % (paxArray[0],pax1note,pax2note)
        printHeadings(longAction, bookingId, UserPoolId, embarkDate=embarkDate,embarkNote=embarkNote)
        r = doAddUser(un=userName, upi=UserPoolId, fName=fName, lName=lName,
                   dates={"departureDate":departureDate,"embarkDate":embarkDate},paxArray=paxArray)
        if NON_FATAL_WARNING in r:
            user_count -= 1
            print("    ADDING USER encountered warning %s\n      " % r,end='')
            doListUsers(Filter=userName)
        print("DONE add-user; count: %s" % (user_count))
        if FATAL_ERROR_CODE in r:
            print("DEBUG: doAddUser returned %s" % (r))
            sys.exit(1)  # fatal exit

    elif longAction == "add-attribs":
        cog_response = cog_client.list_users(UserPoolId=UserPoolId,Filter='email ^= "%s"' % bookingId,Limit=60)
        try:
            attribs = cog_response["Users"][0]["Attributes"]
        except Exception as e:
            logger.warning(" Caught exception: %s (Cognito response: %s)" % (e, cog_response))
            sys.exit(0)  # final/global exit
        logger.debug("Attributes: " + json.dumps(attribs,indent=2))
        user_GUID = None
        # sys.exit(0)  # final/global exit
        UserAttributes=[{"Name":args.attrib_name,"Value":args.attrib_val}]

        for attr in sorted(attribs,key=lambda d:d.get("Name",'a'),reverse=False):  # sort by "Name"
            logger.debug("attr: " + json.dumps(attr,indent=2))
            user_GUID = attr["Value"] if attr["Name"] == "sub" else user_GUID  # 'userGUID aka "sub"'

        r = doAddAttribs(UserPoolId=UserPoolId,Username=user_GUID,UserAttributes=UserAttributes)
        sys.exit(0)  # final/global exit


    print("")  # last action
# EOF