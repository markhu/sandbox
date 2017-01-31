#!/usr/bin/env groovy

// @Grab doesn't work under Maven --find a way to conditionalize it instead of commenting out...
// @Grab(group='org.spockframework', module='spock-core', version='1.0-groovy-2.4')  // @Grapes()
import spock.lang.* ;  // pulls in non-defaults like @Unroll() and @Requires()

// @Grab(group='org.codehaus.groovy.modules.http-builder', module='http-builder', version='0.7')
import groovyx.net.http.RESTClient


class GroovyRestClientSpecTest extends spock.lang.Specification {
  @Shared
// def client = new RESTClient("http://localhost:8080/beacon/")
  def client = new RESTClient("http://localhost:8080/")  // temp Jenkins stub...

  def "initial HTTP call to health"() {
    given: "service is running"
      def tbd = "setup"
    when: "check health.html page"
      // def response = client.get(path : "health.html")
      def response = client.get(path : "login")  // temp Jenkins stub
    then: "assert response contains specific data"
      with(response)
      {
          // data.text == "full text"
          status == 200
          println "INFO: headers: " + headers.text
      }
      println "PASS: ok"
      println "INFO: data.text: " + response.data.text
      println 'INFO: getData(): """\n' + response.getData() + '\n"""'
  }

}  

// EOF $Id: test-spock.groovy 127386 2016-11-15 00:13:05Z mhudson $
