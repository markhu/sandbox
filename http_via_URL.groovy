#!/usr/bin/env groovy

@Grab(group='org.spockframework', module='spock-core', version='1.1-groovy-2.4')  // @Grapes()
import spock.lang.*

// Hit 'Run Script' below
class MyFirstSpec extends Specification {
  def "let's try this!"() {
    when: "call a path that returns JSON"
      // def response = "https://httpbin.org/get".toURL()  // Groovy
      def response = new URL("http://httpbin.org/get")  // implicit java.net.URL
      def json_map = new groovy.json.JsonSlurper().parseText(response.text)  // getText())
      println "raw response.text: ${response.text}"
      println "JSON parseText: ${json_map}"
    then:
      response.text =~ /"args"/
      response.text =~ /"headers"/
  }
}

