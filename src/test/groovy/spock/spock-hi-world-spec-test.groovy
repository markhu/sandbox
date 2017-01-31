#!/usr/bin/env groovy

/*
@Grapes([
@Grab(group='org.spockframework', module='spock-core', version='1.0-groovy-2.4')  // @Grapes()
])
*/
import spock.lang.*  // pulls in non-defaults like @Unroll() and @Requires()

class HelloSpockSpec extends spock.lang.Specification {

  def "example single assertion about Spock"() {
    given:  "setup initial data"
      def attrStr = "logical"
    when:   "tweak data"
      def SpockAttr = attrStr
    then:   "assertions"
      SpockAttr == 'logical'
      println "0. PASS: ok -- SpockAttr == 'logical'"
  }

}

// EOF

