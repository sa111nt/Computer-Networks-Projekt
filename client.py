import tkinter as tk
from tkinter import *
import socket
import time
import threading
import sys
import argparse


class Client:
    def __init__(self, port=1234, ip=socket.gethostbyname("127.0.0.1")):
        self.parser = argparse.ArgumentParser()
        self.parser.add_argument("ip", help="ip")
        self.parser.add_argument("port", help="port")
        self.args = self.parser.parse_args()

        self.mainbuffer = ""
        self.server_ip = socket.gethostbyname(self.args.ip)
        self.server_port = int(self.args.port)
        self.server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.server.connect((self.server_ip, self.server_port))
        self.read_thread = threading.Thread(target=self.receive_messages)
        self.read_thread.start()
        self.handle_msg_thread = threading.Thread(target=self.manageMessages)
        self.handle_msg_thread.start()

    def send_message(self, message):
        encoded_message = message.encode("utf-8")
        self.server.send(encoded_message)

    def receive_messages(self):
        while True:
            try:
                buffer = self.server.recv(1024).decode("utf-8")
                if len(buffer) > 0:
                    self.mainbuffer += buffer
            except:
                self.server.close()
                sys.exit()

    def manageMessages(self):
        while True:
            for i in range(len(self.mainbuffer)):
                if self.mainbuffer[i] == '\n':
                    manageMessage(self.mainbuffer[:i])
                    self.mainbuffer = self.mainbuffer[i + 1:]
                    break


client = Client()


class App(tk.Tk):
    def __init__(self, *args, **kwargs):
        self.countSeconds = 0
        self.counter_thread = threading.Thread(target=self.counter)
        self.counter_thread.start()

        tk.Tk.__init__(self, *args, **kwargs)
        self.title("Państwa-miasta")
        self.geometry("800x500")
        self.configure(bg="#f0f0f0")

        container = tk.Frame(self, bg="#f0f0f0")
        container.pack(side="top", fill="both", expand=True)
        container.grid_rowconfigure(0, weight=1)
        container.grid_columnconfigure(0, weight=1)
        self.frames = {}
        for F in (NicknameFrame, GameFrame):
            page_name = F.__name__
            frame = F(parent=container, controller=self)
            self.frames[page_name] = frame
            frame.grid(row=0, column=0, sticky="nsew")

        self.show_frame('NicknameFrame')

    def show_frame(self, page_name):
        frame = self.frames[page_name]
        frame.tkraise()

    def counter(self):
        while True:
            if self.countSeconds != 0:
                self.frames['GameFrame'].time_label.config(text="Czas: " + str(self.countSeconds) + "s")
                time.sleep(1)
                self.countSeconds -= 1

    def sendNickname(self, entry):
        nickname = entry.get()
        client.send_message('N' + nickname + '\n')

    def changeNicknameLabel(self):
        self.frames['NicknameFrame'].error_label.config(text='Podany nick już istnieje', fg="red")

    def updateCheckBox(self, list):
        list = list[:len(list) - 1].split(";")
        self.frames['GameFrame'].listbox.delete(0, END)
        for i in list:
            self.frames['GameFrame'].listbox.insert(self.frames['GameFrame'].listbox.size() + 1, i)

    def gameStarted(self, letter):
        self.frames['GameFrame'].button["state"] = "normal"
        self.countSeconds = 30
        self.frames['GameFrame'].letter_label.config(text='Litera to: ' + str(letter))
        self.frames['GameFrame'].country_entry.delete(0, END)
        self.frames['GameFrame'].city_entry.delete(0, END)
        self.frames['GameFrame'].name_entry.delete(0, END)

    def sendAnswers(self, country_entry, city_entry, name_entry):
        country = country_entry.get()
        city = city_entry.get()
        name = name_entry.get()
        self.frames['GameFrame'].button["state"] = "disabled"
        client.send_message('A' + country + ";" + city + ";" + name + '\n')


class NicknameFrame(tk.Frame):
    def __init__(self, parent, controller):
        tk.Frame.__init__(self, parent, bg="#f0f0f0")
        self.controller = controller

        self.nickname_label = tk.Label(self, text="Podaj nick", bg="#f0f0f0", font=("Helvetica", 14))
        self.nickname_label.pack(pady=(20, 10))

        self.nickname_entry = tk.Entry(self, font=("Helvetica", 12))
        self.nickname_entry.pack(pady=(0, 20), ipadx=10, ipady=5)

        self.nickname_button = tk.Button(self, text="Potwierdź", command=lambda: controller.sendNickname(self.nickname_entry))
        self.nickname_button.pack(pady=(0, 20))

        self.error_label = tk.Label(self, text="", bg="#f0f0f0", fg="red", font=("Helvetica", 12))
        self.error_label.pack()


class GameFrame(tk.Frame):
    def __init__(self, parent, controller):
        tk.Frame.__init__(self, parent, bg="#f0f0f0")
        self.controller = controller

        self.listbox = tk.Listbox(self, font=("Helvetica", 12), bg="white", selectbackground="#a6a6a6")
        self.listbox.pack(side=tk.LEFT, padx=20, pady=20, fill=tk.BOTH, expand=True)

        right_frame = tk.Frame(self, bg="#f0f0f0")
        right_frame.pack(side=tk.RIGHT, padx=20, pady=20, fill=tk.BOTH, expand=True)

        self.letter_label = tk.Label(right_frame, text="Litera to:", bg="#f0f0f0", font=("Helvetica", 14))
        self.letter_label.pack(pady=(0, 10))

        self.time_label = tk.Label(right_frame, text="Czas: 0s", bg="#f0f0f0", font=("Helvetica", 14))
        self.time_label.pack(pady=(0, 20))

        self.country_label = tk.Label(right_frame, text="Państwo", bg="#f0f0f0", font=("Helvetica", 12))
        self.country_label.pack(pady=(0, 5))

        self.country_entry = tk.Entry(right_frame, font=("Helvetica", 12))
        self.country_entry.pack(pady=(0, 10), ipadx=10, ipady=5)

        self.city_label = tk.Label(right_frame, text="Miasto", bg="#f0f0f0", font=("Helvetica", 12))
        self.city_label.pack(pady=(0, 5))

        self.city_entry = tk.Entry(right_frame, font=("Helvetica", 12))
        self.city_entry.pack(pady=(0, 10), ipadx=10, ipady=5)

        self.name_label = tk.Label(right_frame, text="Imię", bg="#f0f0f0", font=("Helvetica", 12))
        self.name_label.pack(pady=(0, 5))

        self.name_entry = tk.Entry(right_frame, font=("Helvetica", 12))
        self.name_entry.pack(pady=(0, 20), ipadx=10, ipady=5)

        self.button = tk.Button(right_frame, text="Zatwierdź", command=lambda: controller.sendAnswers(self.country_entry, self.city_entry, self.name_entry))
        self.button.pack()
        self.button["state"] = "disabled"


def manageMessage(msg):
    if msg == 'valid':
        app.show_frame('GameFrame')
    elif msg == 'invalid':
        app.changeNicknameLabel()
    elif msg[:5] == 'start':
        app.gameStarted(msg[5])
    else:
        app.updateCheckBox(msg)


if __name__ == "__main__":
    app = App()
    app.mainloop()
