#!/usr/bin/env python3

# pass through command-line args as SQL parameters, to SQL files.

import argparse
import os
import sys

import pyodbc 

sqlQuery_default="""USE VCLNVSQADB;
  SELECT TOP 20 Sales.Invoice,pax.lName,pax.fName,fName2,RecAmt,Curr,
                           FORMAT(Cruise.FMDATE, 'yyyy-MM-dd') as FMDATE,
                          epsilonmp.contactID,cruise.cruise,cruise.crname,Sales.status
  FROM Sales INNER JOIN Cruise ON Sales.Cruise = CRUISE.Cruise
       INNER JOIN dbo.Pax ON Pax.INVOICE = Sales.Invoice
       INNER JOIN dbo.epsilonmp ON epsilonmp.mpcard = Pax.MPCARD
  WHERE dbo.CRUISE.FMDATE >= convert(varchar, getdate() +7)
      -- OR Sales.Invoice IN(4888778)
  """
#         -- AND Sales.Invoice IN(${arg1})


SQLCMDHOST = os.environ.get("SQLCMDHOST","redacted.nosmokingrc.com")
server = "tcp:%s,1441" % (SQLCMDHOST)
database = os.environ.get("SQLCMDDBNAME", "VCLNVSQADB")
username = os.environ.get("SQLCMDUSER", "# Requires (read-only) SQL bot credentials")
password = os.environ.get("SQLCMDPASSWORD", "# secret environment variable (Jenkins-able)")

parser = argparse.ArgumentParser( description="SQL query via Python l")
parser.add_argument("-i","--file", default=None, help="SQL query file")
parser.add_argument("-d","--database", default=database, help="SQL db")
parser.add_argument("-S","--server", default=server, help="SQL server")
parser.add_argument("--query", default=sqlQuery_default, help="raw SQL")
parser.add_argument("-v","--verbose", action="store_true", help="verbose = print more info")
args = parser.parse_args()

queryText = args.query

cnxn = pyodbc.connect('DRIVER={' + pyodbc.drivers()[0]  + '};SERVER='+args.server+';DATABASE='+args.database+';UID='+username+';PWD='+ password)
cursor = cnxn.cursor()

if queryText == sqlQuery_default and args.file is None: 
    if args.verbose:
        print(queryText)
else:
    try:
      queryText = open(args.file, "r").read()
    except:
      queryText = args.query

    with sys.stderr as io:
      print("input file %s contains:\n%s" % (args.file, queryText), file=io)
      print("  future: %s, curr: %s, office: %s" % (args.future, args.curr, args.office), file=io)
      print("  dirFlag: %s, recamt: %s, recAmtMax: %s" % (args.dirflag, args.recamt, args.recamtmax), file=io)
      print('DRIVER={ODBC Driver 17 for SQL Server};SERVER='+server+';DATABASE='+database+';UID='+username+';PWD='+ password)

try:
      r = cursor.execute(queryText)
      print("DEBUG: r: %s" % r)
      print("DEBUG: dir(r): %s" % dir(r))
      print("DEBUG: r.description: %s" % r.description)
except Exception as e:
      r = cursor.execute(queryText)
      print("ERROR!: %s" % (e))

delim = ','
columns = [(column[0]) for column in r.description]
# print('\t'.join(columns))
for c in columns:
    print("%-13s" % (c + delim), end=' ')
print('')

for row in cursor:
    for y in row:
        print("%-13s" % (str(y)[:13].strip() + delim), end=' ')
    print('')

