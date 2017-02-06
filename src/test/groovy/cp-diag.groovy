#!/usr/bin/env groovy

// Print all the environment variables.
// System.getenv().each{ println it } 

// Individual environment variables of interest:
println "env HOME:       " + System.getenv()['HOME']
println "env USER:       " + System.getenv()['USER']
println "env CATALINA_BASE: " + System.getenv()['CATALINA_BASE']
println "env CATALINA_HOME: " + System.getenv()['CATALINA_HOME']
println "env CLASS_PATH: " + System.getenv()['CLASS_PATH']
println "env GROOVY_PATH:" + System.getenv()['GROOVY_PATH']
println "env JAVA_HOME:  " + System.getenv()['JAVA_HOME']

println "ClassPath diagnostic --this affects both lib import, as well as file read"
this.class.classLoader.rootLoader.URLs.each{ println it }  

