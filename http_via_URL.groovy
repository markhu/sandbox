#!/usr/bin/env groovy

@Grab(group='org.spockframework', module='spock-core', version='1.1-groovy-2.4')  // @Grapes()
import spock.lang.*

// Hit 'Run Script' below
class MyFirstSpec extends Specification {
  def "let's try this!"() {
    when: "call a path that returns JSON"
      def url = "http://httpbin.org/get".toURL()  // toURL is Groovy for implicit java.net.URL 
      println "raw url: ${url}"
      print   "url.content: (raw)"
      println  url.content
      println "url.content: (string-ified)\n${url.content}"
      println "url.text: (raw) is a Groovy value-added syntactical sugar"
   // println  url.text
      println "url.text == url.content: (string-ified) " + ("${url.content}" == "${url.text}")
      println "url.metaClass ... " + url.metaClass.methods*.name.sort().unique()

      println "----"
      def connection = url.openConnection()  // URLConnection
      println "raw connection: ${connection}"
      println "connection.ResponseCode: " + connection.responseCode
      println "connection.metaClass ... " + connection.metaClass.methods*.name.sort().unique()
      println "connection.headerFields: " + connection.headerFields

      println "----"
      def json_map = new groovy.json.JsonSlurper().parseText(url.text)  // getText())
      println "JSON parseText: ${json_map}"

    then:
      url.text =~ /"args"/
      "${url.content}" =~ /"headers"/
      url.text =~ /"headers"/
      connection.responseCode == 200
  }
}

