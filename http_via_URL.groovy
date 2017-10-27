#!/usr/bin/env groovy

@Grab(group='org.spockframework', module='spock-core', version='1.0-groovy-2.4')  // @Grapes()
import spock.lang.*

// Hit 'Run Script' below
class MyFirstSpec extends Specification {
  def "let's try this!"() {
    when: "call a path that returns JSON"
      def response = "https://httpbin.org/get".toURL()
      println response.text
    then:
      response.text =~ /"args"/
      response.text =~ /"headers"/
  }
}

