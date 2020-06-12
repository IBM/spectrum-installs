import sys, getopt
from os import path
from xml.etree import ElementTree as et

def updateResourcePlan(inputXml, outputXml, consumers):
	if not path.exists(inputXml):
		print("Error: Input file %s does not exist" % inputXml)
		sys.exit(1)
	et.register_namespace('', "http://www.platform.com/ego/2005/05/schema")
	tree = et.parse(inputXml)
	ns = '{http://www.platform.com/ego/2005/05/schema}'
	for consumer in consumers.split(","):
		share = tree.find("./"+ns+"DistributionTree[@DistributionTreeName='ComputeHosts']//"+ns+"Consumer[@ConsumerName='"+consumer+"']/"+ns+"DistributionPolicies/"+ns+"SharingPolicy/"+ns+"Shares[@Type='ratio']")
		if share is None:
    			print("Error: Cannot find consumer %s" % consumer)
            		sys.exit(1)
        	else:
            		share.text="0"
	tree.write(outputXml)

def print_help():
	print("%s -i <input> -o <output> -c <consumers>" % sys.argv[0])
	print("This script will update the share ratio of all consumers specified (\"-c\" argument, comma separated list) to 0, from the resource plan specified (\"-i\" argument), and write it in the output file (\"-o\" argument)")
	sys.exit(1)

def main(argv):
	inputXml = ''
	outputXml = ''
	consumers = ''
	try:
		opts, args = getopt.getopt(argv,"hi:o:c:",["help","input=","output=","consumers="])
	except getopt.GetoptError:
		print_help()
	for opt, arg in opts:
		if opt in ('-h', "--help"):
			print_help()
		elif opt in ("-i", "--input"):
			inputXml = arg
		elif opt in ("-o", "--output"):
			outputXml = arg
		elif opt in ("-c", "--consumers"):
			consumers = arg
	if inputXml=='' or outputXml=='' or consumers=='':
		print_help()
	else:
		updateResourcePlan(inputXml, outputXml, consumers)

if __name__ == "__main__":
   main(sys.argv[1:])
