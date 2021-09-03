package main;

import java.io.File;

public class Rename
{
	public static void main(String[] args) throws Exception
	{
		int size = 500;
		boolean rc = false;
		int maxNodes = 100;
		
		for(int nodes = 10; nodes <= maxNodes; nodes+=10)
		{
			for(int i = 3; i < 4; i++)
			{
				File f;
				if(rc)
				{
					f = new File("/home/krassus/Dokumente/TU-Darmstadt/Masterarbeit/Auswertungen/" + size + "x" + size + "/buildMax_rc/" + nodes + "/output_" + i + "_" + nodes + ".log");
					if(f.exists())
						f.renameTo(new File("/home/krassus/Dokumente/TU-Darmstadt/Masterarbeit/Auswertungen/" + size + "x" + size + "/buildMax_rc/" + nodes + "/output_" + i + ".log"));
				}
				else
				{
					f = new File("/home/krassus/Dokumente/TU-Darmstadt/Masterarbeit/Auswertungen/" + size + "x" + size + "/normal/" + nodes + "/output_" + i + "_" + nodes + ".log");
					if(f.exists())
						f.renameTo(new File("/home/krassus/Dokumente/TU-Darmstadt/Masterarbeit/Auswertungen/" + size + "x" + size + "/normal/" + nodes + "/output_" + i + ".log"));
				}
			}
		}
	}
}
