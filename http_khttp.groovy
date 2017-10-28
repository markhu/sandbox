#!/usr/bin/env groovy

@GrabResolver(name='jitpack.io', root='https://jitpack.io')
@Grab('com.github.jkcclemens:khttp:-SNAPSHOT')
import khttp.KHttp ;  // like Python's HTTP Requests

def response = KHttp.get("http://httpbin.org/get")
println "response.url: ${response.url}"
println "response.getProperties().keySet(): ${response.getProperties().keySet()}"
println "response.statusCode: ${response.statusCode}"
println "response.headers['Content-Type']: ${response.headers['Content-Type']}"
println "response.headers['Content-Length']: ${response.headers['Content-Length']}"
println "response.text length: ${response.text.length()}"

def filename = 'result.html'
println "Writing output to ${filename}"
new File(filename).text = response.text

