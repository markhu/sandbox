#!/usr/bin/env groovy

// Maven can't work with @Grab --find a way to conditionalize it instead of commenting out...
@Grab(group='org.spockframework', module='spock-core', version='1.0-groovy-2.4')  // @Grapes()
import spock.lang.* ;  // pulls in non-defaults like @Unroll() and @Requires()

@Grab(group='org.codehaus.groovy.modules.http-builder', module='http-builder', version='0.7.2')
import groovyx.net.http.RESTClient

import groovy.json.JsonOutput

class GroovyRestClientSpecTest extends spock.lang.Specification {
  @Shared
  def baseURL = "https://httpbin.org/"
  def client = new RESTClient(baseURL)

  def "initial HTTP call to health"() {
    given: "service is running"
      def tbd = "setup"
    when: "call a path that returns JSON"
      def response = client.get( path : "get")  // urlpath to append to base
    then: "assert response contains specific data"
      with(response)
      {
          // response.text == "full text"
          status == 200
          println "INFO: data.headers:  " + data.headers
          println "INFO: data.url: " + data.url
      }
      assert response.data.url == "${baseURL}get"
      println "INFO: data: " + response.data
      println "INFO: data.text: " + response.data.text
      println 'INFO: getData(): """\n' + response.getData() + '\n"""'
      println "INFO: data.keySet(): " + response.data.keySet()
      println "INFO: getData().get('url'): " + response.getData().get('url')
      println "INFO: data.get('url'): " + response.data.get('url')
      println "INFO: data.get('getFake'): " + response.data.get('getFake','alt')
      println "INFO: data.getOrDefault('fake'): " + response.data.getOrDefault('fake','fake does not exist')
      def json = JsonOutput.toJson(response.data)
      println "INFO: JsonOutput.toJson(response.data): " + json
      println "INFO: JsonOutput.toJson(response.data) ... prettyPrint(): " + JsonOutput.prettyPrint(json)
  }

}  

// EOF $Id: test-spock.groovy 127386 2016-11-15 00:13:05Z mhudson $
