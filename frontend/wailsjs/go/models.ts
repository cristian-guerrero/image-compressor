export namespace main {
	
	export class JobDisplay {
	    id: string;
	    sourcePath: string;
	    outputPath: string;
	    status: string;
	    progress: number;
	    totalFiles: number;
	    doneFiles: number;
	    currentFile: string;
	
	    static createFrom(source: any = {}) {
	        return new JobDisplay(source);
	    }
	
	    constructor(source: any = {}) {
	        if ('string' === typeof source) source = JSON.parse(source);
	        this.id = source["id"];
	        this.sourcePath = source["sourcePath"];
	        this.outputPath = source["outputPath"];
	        this.status = source["status"];
	        this.progress = source["progress"];
	        this.totalFiles = source["totalFiles"];
	        this.doneFiles = source["doneFiles"];
	        this.currentFile = source["currentFile"];
	    }
	}

}

