#!/usr/bin/env groovy

@Grab('org.apache.httpcomponents:httpclient:4.5.3')
import org.apache.http.impl.client.DefaultHttpClient;
import org.apache.http.client.methods.HttpGet 

def httpClient = new DefaultHttpClient() 
def url = 'http://www.google.com/search?q=Groovy'
def httpGet = new HttpGet(url) 

def httpResponse = httpClient.execute(httpGet) 
def contentText = httpResponse.entity.content.text  // store self-closing stream
println "httpResponse.getStatusLine(): ${httpResponse.getStatusLine()}"
// println "httpResponse.getStatusLine().getStatusCode(): ${httpResponse.getStatusLine().getStatusCode()}"
println "content.text length: ${contentText.length()} of ${url}"

def filename = 'result.html'
println "Writing output to ${filename}"
new File(filename).text = contentText

