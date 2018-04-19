#!/usr/bin/env groovy

def env = System.getenv()
println "env['MY_ENV_VAR']) : '" +
         env['MY_ENV_VAR'] + "'"
println "Boolean.valueOf(env['MY_ENV_VAR']) : " +
         Boolean.valueOf(env['MY_ENV_VAR'])
println env['TERM']?.contains('term')

try {
  println env['MY_ENV_VAR']?.contains('f')
} catch(Exception e) {
  println "CAUGHT: ${e}"
}
