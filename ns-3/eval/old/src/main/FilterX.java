package main;

import java.io.BufferedReader;
import java.io.FileReader;
import java.io.PrintStream;

public class FilterX
{
	public static void main(String[] args) throws Exception
	{
		int mode = 3;
		int maxNodes = 100;
		int size = 500;
		boolean remote = false;
		
		/*FileReader frx = new FileReader("/home/krassus/Schreibtisch/tmp/output_" + mode + "_40.log");
		
		try(BufferedReader br = new BufferedReader(frx))
		{	
			String line;
			while((line = br.readLine()) != null)
			{
				if(line.startsWith("CODE:"))
					System.out.println(line.substring(6).replace('.', ','));
			}
		}
		System.exit(0);*/
		
		try(PrintStream ps = new PrintStream("output_statistics.log"))
		{
			for(int nodes = 10; nodes <= maxNodes; nodes+=10)
			{
				FileReader fr;
				if(remote)
					fr = new FileReader("/home/krassus/Dokumente/TU-Darmstadt/Masterarbeit/Auswertungen/" + size + "x" + size + "/normal_rc/" + nodes + "/output_" + mode + ".log");
				else
					fr = new FileReader("/home/krassus/Dokumente/TU-Darmstadt/Masterarbeit/Auswertungen/" + size + "x" + size + "/normal/" + nodes + "/output_" + mode + ".log");
				
				try(BufferedReader br = new BufferedReader(fr))
				{	
					String line;
					while((line = br.readLine()) != null)
					{
						if(line.startsWith("CODE:"))
							ps.println(line.substring(6).replace('.', ','));
					}
				}
			}
		}
	}
}