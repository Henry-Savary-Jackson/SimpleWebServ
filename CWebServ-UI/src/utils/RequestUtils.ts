import axios from "axios"

export var webroot = "test"
export var backend_url = "http://localhost:8000"

axios.defaults.httpVersion = 1;
axios.defaults.decompress = true;

export interface FileData {
    file_name: string
    mimetype: string
    isDir: boolean
    mod_time: number
};

export async function getCSRF() {
    let response = await axios.get("/csrf", { withCredentials: true });
    axios.defaults.headers["X-CSRF-TOKEN"] = response.data;
    return response.data;
}

export async function login(username:string, password:string, csrf:string) {
    let formdata = new URLSearchParams();
    formdata.append("username", username);
    formdata.append("password", password);
    formdata.append("csrf", csrf);

    let response = await axios.post("/login", formdata, { withCredentials: true });
    return response.data;
}
export async function signUp(username:string, password:string, csrf:string) {
    let formdata = new URLSearchParams();
    formdata.append("username", username);
    formdata.append("password", password);
    formdata.append("csrf", csrf);

    let response = await axios.post("/signup", formdata, { withCredentials: true });
    return response.data;
}

export async function postFile(path:string, file_name:string, file_blob:ArrayBuffer, csrf:string): Promise<FileData> {
    let response = await axios.post(`${path}/${file_name}`, file_blob, { withCredentials: true })
    return convertLineToFileData(response.data)
}

function convertLineToFileData(line:string){
    const values = line.trim().split(":")
    return { isDir: values[0] == "DIR", mimetype: values[0], mod_time: parseInt(values[1]), file_name: values[2] }
}

export async function listDirectory(path:string, dir_name:string) : Promise<FileData[]> {

    let response = await axios.get(`${path}/${dir_name}`, { params: { list: "true" }, withCredentials: true })

    let files : FileData[] = response.data.split("\n").map(convertLineToFileData)

    return files
}


export async function postDirectory(path:string, directory_name:string, csrf:string) {
    let response = await axios.post(`${path}/${directory_name}`, null, { withCredentials: true })
    return response.data
}
export async function deleteFile(path:string, file_name:string) {
    let response = await axios.delete(`${path}/${file_name}`, { withCredentials: true })
    return response.data
}
