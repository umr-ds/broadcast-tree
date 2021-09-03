package main;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileReader;
import java.io.PrintStream;

public class Filter
{
	public static void main(String[] args) throws Exception
	{
		String[] nids = "23".split(",");
		int[] nodeIDs = new int[nids.length];
		for(int i = 0; i < nodeIDs.length; i++)
			nodeIDs[i] = Integer.parseInt(nids[i], 16) - 1;
		nodeIDs = new int[20];
		for(int i = 0; i < nodeIDs.length; i++)
			nodeIDs[i] = i;
		
		try(BufferedReader br = new BufferedReader(
				new FileReader("/home/krassus/Programme/NS-3.30.1/ns-3.30.1/scratch/broadcast/output_0.log")))
		{
			PrintStream[] ps = new PrintStream[nodeIDs.length];
			for(int i = 0; i < nodeIDs.length; i++)
				ps[i] = new PrintStream(new File("output_" + nodeIDs[i] + ".log"));
			
			String line;
			boolean[] lastLineUsed = new boolean[nodeIDs.length];
			
			int lines = 0;
			long startTime = System.currentTimeMillis();
			
			while((line = br.readLine()) != null)
			{
				for(int i = 0; i < nodeIDs.length; i++, lines++)
				{
					if(line.startsWith("[Node " + nodeIDs[i] + "]:") || line.startsWith("[Node " + nodeIDs[i] + " /") ||
					   line.startsWith("[Node " + nodeIDs[i] + "/") || (line.startsWith("+") &&
					   Integer.parseInt(line.substring(line.indexOf('s') + 2, line.indexOf(' ', line.indexOf(' ') + 1))) == nodeIDs[i]))
					{
						ps[i].println(line);
						lastLineUsed[i] = true;
					}
					else if(lastLineUsed[i] && !line.startsWith("[") && !line.startsWith("+"))
					{
						if(line.equals("<========== END OF SIMULATION ==========>"))
							break;
						ps[i].println(line);
						lastLineUsed[i] = line.startsWith("\t");
					}
					else
						lastLineUsed[i] = false;
				}
				
			}
			
			System.out.println(lines + " Lines scanned in " + (System.currentTimeMillis() - startTime) + "ms");
			
			for(int i = 0; i < nodeIDs.length; i++)
				ps[i].close();
		}
	}
}