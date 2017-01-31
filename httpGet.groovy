#!/usr/bin/env groovy

@Grab('org.apache.httpcomponents:httpclient:4.2.1')
import org.apache.http.impl.client.DefaultHttpClient;
import org.apache.http.client.methods.HttpGet 

def httpClient = new DefaultHttpClient() 
def url = 'http://www.google.com/search?q=Groovy'
def httpGet = new HttpGet(url) 

def httpResponse = httpClient.execute(httpGet) 

new File('result.html').text = httpResponse.entity.content.text

