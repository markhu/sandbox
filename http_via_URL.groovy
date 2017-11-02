#!/usr/bin/env groovy

@Grab(group='org.spockframework', module='spock-core', version='1.1-groovy-2.4')  // @Grapes()
import spock.lang.*

// Hit 'Run Script' below
class MyFirstSpec extends Specification {
  def "let's try this!"() {
    when: "call a path that returns JSON"
      // def response = "https://httpbin.org/get".toURL()  // Groovy
      def response = new URL("http://httpbin.org/get")  // implicit java.net.URL
      println "raw response: ${response}"
      println "response.text: ${response.text}"
      println "response.metaClass ... " + response.metaClass.methods*.name.sort().unique()

      println "----"
      def connection = response.openConnection()
      println "raw connection: ${connection}"
      println "connection.ResponseCode: " + connection.responseCode
      println "connection.metaClass ... " + connection.metaClass.methods*.name.sort().unique()
      println "connection.headerFields: " + connection.headerFields

      println "----"
      def json_map = new groovy.json.JsonSlurper().parseText(response.text)  // getText())
      println "JSON parseText: ${json_map}"

    then:
      response.text =~ /"args"/
      response.text =~ /"headers"/
      connection.responseCode == 200
  }
}

