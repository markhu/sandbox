#!/usr/bin/env groovy

@Grab(group='org.spockframework', module='spock-core', version='1.0-groovy-2.4')  // @Grapes()
import spock.lang.* ;  // pulls in non-defaults like @Unroll() and @Requires()

@Grab(group='org.codehaus.groovy.modules.http-builder', module='http-builder', version='0.7')
import groovyx.net.http.RESTClient

@Grab(group='com.mashape.unirest', module='unirest-java', version='1.4.9')
import com.mashape.unirest.http.JsonNode
import com.mashape.unirest.http.HttpResponse
import com.mashape.unirest.http.Unirest


class UniRestClientSpec extends spock.lang.Specification {

  @Shared
  def base_url = "http://localhost:8080/api/json?pretty=true"  // temp Jenkins stub...

  def "initial HTTP call to health"() {
    given: "service is running"
      def tbd = 'spin something up'
    when: "check health.html page"
      // HttpResponse<JsonNode> response = Unirest.get(base_url).basicAuth("bott", "5225577816821dbd7b2878c2d9f417cf").asJson();  // asString();
      def response = Unirest.get(base_url).basicAuth("bott", "5225577816821dbd7b2878c2d9f417cf").asString();
      print "DEBUG: response: " + response + '\n'
      // def response = client.get(path : "health.html")
    then: "assert response contains specific data"
      with(response)
      {
          status == 200
          println "INFO: headers: " + headers.text
          // data.text == "full text"
          // body.grep("blah") == um...
      }
      println "PASS: ok"
      println 'INFO: getStatus(): """\n' + response.getStatus() + '\n"""'
      println 'INFO: response.body: """\n' + response.body + '\n"""'
//    println 'INFO: response.getBody(): """\n' + response.getBody() + '\n"""'
//    println 'INFO: response.dump(): """\n' + response.dump() + '\n"""'
//    println 'INFO: response...primaryView: """\n' + response.body.grep("primaryView") + '\n"""'
  }

}  

// EOF $Id: test-spock.groovy 127386 2016-11-15 00:13:05Z mhudson $
